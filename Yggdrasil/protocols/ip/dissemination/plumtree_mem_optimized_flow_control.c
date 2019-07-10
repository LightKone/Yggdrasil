//
// Created by Pedro Akos on 2019-06-17.
//

#include "plumtree_mem_optimized_flow_control.h"

#define GC_TIME_S 600 //maybe this should be a parameter

typedef enum __plumtree_msg_types {
    GOSSIP,
    PRUNE,
    IHAVE,
    GRAFT,
    ANNOUNCEMENT,
    //only for large body requests
    GOSSIP_ACK,
}plumtree_msg_types;


typedef struct __peer {
    IPAddr ip;
}peer;


static bool equal_addr(IPAddr* ip1, IPAddr* ip2) {
    return (strcmp(ip1->addr, ip2->addr) == 0 && ip1->port == ip2->port);
}

static bool equal_peer_addr(peer* p, IPAddr* ip) {
    return  equal_addr(&p->ip, ip);
}

static bool equal_peer(peer* p1, peer* p2) {
    return (strcmp(p1->ip.addr, p2->ip.addr) == 0 && p1->ip.port == p2->ip.port);
}

static peer* create_empty_peer() {
    peer* p = malloc(sizeof(peer));
    bzero(p->ip.addr, 16);
    return p;
}


static void destroy_peer(peer* p) {
    free(p);
}

typedef struct __lazy_queue_item {
    peer* node;
    int mid; //mid can be calculate through hash functions
}lazy_queue_item;

static bool equal_lqi(lazy_queue_item* lqi1, lazy_queue_item* lqi2) {
    return lqi1->mid == lqi2->mid && equal_peer(lqi1->node, lqi1->node);
}

static bool equal_mid(lazy_queue_item* lqi, int* mid) {
    return lqi->mid == *mid;
}

static bool equal_node(lazy_queue_item* lqi, peer* p) {
    return equal_peer(lqi->node, p);
}

static lazy_queue_item* create_lazy_queue_item(peer* node, int mid) {
    lazy_queue_item* i = malloc(sizeof(lazy_queue_item));
    i->node = node;
    i->mid = mid;
    return i;
}


typedef struct __mid {
    int hash;
    unsigned requestor_id; //need this if it is to be resend
    void* msg_payload;
    unsigned short msg_payload_size;
    struct timespec received;
}message_item;

static bool equal_message_item(message_item* m, int* mid){
    return m->hash == *mid;
}

static message_item* create_msg_item(int mid, unsigned short requestor_id, void* msg_payload, unsigned short msg_length) {
    message_item* m = malloc(sizeof(message_item));
    m->hash = mid;
    m->requestor_id = requestor_id;
    m->msg_payload = malloc(msg_length);
    memcpy(m->msg_payload, msg_payload, msg_length);
    m->msg_payload_size = msg_length;
    clock_gettime(CLOCK_MONOTONIC, &m->received);
    return m;
}

static void destroy_message_item(message_item* m) {
    free(m->msg_payload);
}

static bool expired(message_item* m) { //TODO better this
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    return (now.tv_sec - m->received.tv_sec > GC_TIME_S);
}

typedef struct __timer {
    int mid;
    YggTimer* t;
}timeouts;

static bool equal_timer(timeouts* t, int* mid) {
    return t->mid == *mid;
}

typedef struct __pipelined_msg {
    message_item* mid;
    IPAddr ip;
    bool isGraft;
}pipelined_msg;

static bool equal_pipelined_msg(pipelined_msg* msg, pipelined_msg* it) {
    return equal_addr(&msg->ip, &it->ip) && equal_message_item(msg->mid, &it->mid->hash) && msg->isGraft == it->isGraft;
}

static bool equal_graft_request_sender(pipelined_msg* msg, IPAddr* ip) {
    return equal_addr(&msg->ip, ip) && msg->isGraft == true;
}

static void add_to_pipeline(list* pipeline, message_item* mid, IPAddr* p, bool isGraft) {
    pipelined_msg* msg = malloc(sizeof(pipelined_msg));
    msg->mid = mid;
    msg->ip = *p;
    msg->isGraft = isGraft;
    list_add_item_to_tail(pipeline, msg);
}

static void remove_from_pipeline(list* pipeline, message_item* m, IPAddr* ip, bool isGraft) {
    pipelined_msg torm;
    torm.ip = *ip;
    torm.isGraft = isGraft;
    torm.mid = m;
    pipelined_msg* msg = list_remove_item(pipeline, (equal_function) equal_pipelined_msg ,&torm);

    if(msg)
        free(msg);

}


typedef struct __plumtree_state {
    unsigned short proto_id;

    peer* self;

    list* received; // a list of message identifiers and messages...

    list* eager_push_peers; // a list of neighbours that belong to the spanning tree
    list* lazy_push_peers; // a list of neighbours that dont belong to the spanning tree

    list* lazy_queue; // a list of tuples {mId, node}
    list* missing;

    list* timers; //a list of message identifiers

    int fanout;

    unsigned short timeout_s;
    long timeout_ns;

    int dispatch_msg_size_threashold;

    message_item* ongoing;
    list* missingAcks;
    list* pipeline;

}plumtree_state;


typedef struct __plumtree_request_msg_body_header {
    int mid;
    bool isGraft;
    IPAddr ip;
}plumtree_request_msg_body_header;

static void request_msg_body(message_item* m_item, IPAddr* ip, bool isGraft, short my_id) {

    YggRequest req;
    YggRequest_init(&req, my_id ,m_item->requestor_id, REQUEST, MSG_BODY_REQ);

    plumtree_request_msg_body_header* header = malloc(sizeof(plumtree_request_msg_body_header));

    header->ip = *ip;
    header->mid = m_item->hash;
    header->isGraft = isGraft;

    dissemination_msg_request* r = create_dissemination_msg_request((void*) header);
    dissemmination_request_add_to_header(r->req, m_item->msg_payload, m_item->msg_payload_size);

    req.length = sizeof(dissemination_msg_request);
    req.payload = r;

    deliverRequest(&req);
    YggRequest_freePayload(&req);
}

static void print_tree(plumtree_state* state) {

    printf("PlumTree neighbors:\n");
    printf("\t eager push:\n");
    for(list_item* it = state->eager_push_peers->head; it != NULL; it=it->next) {
        peer* p = (peer*)it->data;
        printf("\t\t %s  %d\n", p->ip.addr, p->ip.port);
    }

    printf("\t lazy push:\n");
    for(list_item* it = state->lazy_push_peers->head; it != NULL; it=it->next) {
        peer* p = (peer*)it->data;
        printf("\t\t %s  %d\n", p->ip.addr, p->ip.port);
    }

}

static void cancel_timer(int mid, plumtree_state* state) {
    timeouts* t = list_remove_item(state->timers, (equal_function) equal_timer, &mid);
    if(t) {
        cancelTimer(t->t);
        YggTimer_freePayload(t->t);
        free(t->t);
        free(t);
    }
}

static void set_timer(int mid, plumtree_state* state) {
    YggTimer* t = malloc(sizeof(YggTimer));
    YggTimer_init(t, state->proto_id, state->proto_id);
    YggTimer_set(t, state->timeout_s, state->timeout_ns, 0, 0);
    YggTimer_addPayload(t, &mid, sizeof(int));

    timeouts* tms = malloc(sizeof(timeouts));
    tms->t = t;
    tms->mid = mid;
    list_add_item_to_tail(state->timers, tms);

    setupTimer(t);
}

static void deliver_msg(unsigned short proto_dest, void* msg_contents, unsigned short msg_size, IPAddr* dest_ip, IPAddr* src_ip) {

    YggMessage m;
    YggMessage_initIp(&m, proto_dest, dest_ip->addr, dest_ip->port);
    memcpy(m.header.src_addr.ip.addr, src_ip->addr, strlen(src_ip->addr));
    m.header.src_addr.ip.port = src_ip->port;
    m.data = msg_contents;
    m.dataLen = msg_size;

#if defined DEBUG | DEBUG_PLUMTREE
    char debug_msg[100];
    bzero(debug_msg, 100);
    sprintf(debug_msg, "delivering message with size: %d to: %d\n", m.dataLen, m.Proto_id);
    ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

    deliver(&m);
}

static plumtree_msg_types decodify(uint16_t type) {
    return ntohs(type);
}

static uint16_t codify(plumtree_msg_types type) {
    return htons(type);
}

static int hash(char* to_hash, unsigned short to_hash_len) {
    return DJBHash(to_hash, to_hash_len);
}


static int generate_mid(void* msg_payload, unsigned short msg_payload_size, unsigned short requestor_id, peer* self) {
    unsigned short to_hash_len = msg_payload_size + sizeof(unsigned short)*2 + 16 + sizeof(time_t) + sizeof(__syscall_slong_t);
    char* to_hash = malloc(to_hash_len);
    memcpy(to_hash, msg_payload, msg_payload_size);
    int off = msg_payload_size;
    memcpy(to_hash+off, &requestor_id, sizeof(unsigned short));
    off += sizeof(unsigned short);
    memcpy(to_hash+off, self->ip.addr, 16);
    off += 16;
    memcpy(to_hash+off, &self->ip.port, sizeof(unsigned short));
    off += sizeof(unsigned short);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    memcpy(to_hash+off, &now.tv_sec, sizeof(time_t)),  off += sizeof(time_t);
    memcpy(to_hash+off, &now.tv_nsec, sizeof(__syscall_slong_t));

    int mid = hash(to_hash, to_hash_len);

#ifdef DEBUG_MID
    int i;
    for (i = 0; i < to_hash_len; i++)
    {
        if (i > 0) printf(":");
        printf("%02X", to_hash[i]);
    }
    printf("\n");
    printf("mid: %d\n", mid);
#endif

    free(to_hash);

    return mid;
}

static void init_msg_header(YggMessage* msg, plumtree_msg_types type, peer* dest, unsigned proto_id) {
    YggMessage_initIp(msg, proto_id, dest->ip.addr, dest->ip.port);
    uint16_t msg_type = codify(type);
    YggMessage_addPayload(msg, (char*) &msg_type, sizeof(uint16_t));
}

static void send_gossip_msg(peer* dest, peer* self, unsigned short requester_id, int mid, dissemination_request* req, unsigned short proto_id) {

#if defined DEBUG | DEBUG_PLUMTREE
    char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending gossip message with mid: %d to: %s %d\n", mid, dest->ip.addr, dest->ip.port);
	ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

    YggMessage msg;
    init_msg_header(&msg, GOSSIP, dest, proto_id);

    uint32_t mid_t = htonl(mid);

    YggMessage_addPayload(&msg, (char*) &mid_t, sizeof(uint32_t));

    uint16_t req_id = htons(requester_id);
    YggMessage_addPayload(&msg, (char*) &req_id, sizeof(uint16_t));

    uint16_t h_size = htons(req->header_size);
    YggMessage_addPayload(&msg, (char*) &h_size, sizeof(uint16_t));

    uint16_t b_size = htons(req->body_size);
    YggMessage_addPayload(&msg, (char*) &b_size, sizeof(uint16_t));

    YggMessage_addPayload(&msg, req->header, req->header_size); //this could be optimized for passing references (but would need to have a dispatcher that could handle this)

    if(req->body_size > 0 && req->body)
        YggMessage_addPayload(&msg, req->body, req->body_size); //this could be optimized for passing references (but would need to have a dispatcher that could handle this)


    dispatch(&msg);

    YggMessage_freePayload(&msg);
}

static void send_gossip_ack(peer* dest, peer* self, unsigned short proto_id) {

#if defined DEBUG | DEBUG_PLUMTREE
    char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending gossip ack message to: %s %d\n", dest->ip.addr, dest->ip.port);
	ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

    YggMessage msg;
    init_msg_header(&msg, GOSSIP_ACK, dest, proto_id);

    dispatch(&msg);

    YggMessage_freePayload(&msg);
}

static void send_prune_msg(peer* dest, peer* self, unsigned short proto_id) {

#if defined DEBUG | DEBUG_PLUMTREE
    char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending prune message to: %s %d\n", dest->ip.addr, dest->ip.port);
	ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

    YggMessage msg;
    init_msg_header(&msg, PRUNE, dest, proto_id);

    dispatch(&msg);

    YggMessage_freePayload(&msg);
}


static void send_graft_msg(peer* node, peer* self, int mid, unsigned short proto_id) {
#if defined DEBUG | DEBUG_PLUMTREE
    char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending graft message: for mid: %d to: %s %d", mid, node->ip.addr, node->ip.port);
	ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

    YggMessage msg;
    init_msg_header(&msg, GRAFT, node, proto_id);

    uint32_t mid_t = htonl(mid);

    YggMessage_addPayload(&msg, (char*) &mid_t, sizeof(uint32_t));

    dispatch(&msg);

    YggMessage_freePayload(&msg);

}

static void build_ihave_entry(YggMessage* msg, int mid) {
    uint32_t mid_t = htonl(mid);

    YggMessage_addPayload(msg, (char*) &mid_t, sizeof(uint32_t));
}

static list* policy(plumtree_state* state) {
    list* l = list_init();

    while(state->lazy_queue->size > 0)
        list_add_item_to_tail(l, list_remove_head(state->lazy_queue));

    return l;
}

static void send_ihave(peer* p, short proto_id, int mid) {
    YggMessage msg;
    init_msg_header(&msg, IHAVE, p, proto_id);
    build_ihave_entry(&msg, mid);

#if defined DEBUG | DEBUG_PLUMTREE
    char debug_msg[100];
    bzero(debug_msg, 100);
    sprintf(debug_msg, "sending ihave %d to: %s %d", mid, msg.header.dst_addr.ip.addr, msg.header.dst_addr.ip.port);
    ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

    dispatch(&msg);
    YggMessage_freePayload(&msg);
}

static void announce_ihave(list* announcements, short proto_id) {

    lazy_queue_item* i = NULL;
    while((i = list_remove_head(announcements)) != NULL) {
        send_ihave(i->node, proto_id, i->mid);
        free(i);
    }

}

static void dispatch_announcements(plumtree_state* state) {
    list* announcements = policy(state);

    announce_ihave(announcements, state->proto_id);

    free(announcements);
}



static void eager_push(dissemination_request* req, unsigned short requester_id, message_item* mid, peer* sender, plumtree_state* state) {
    state->ongoing = mid;

    for(list_item* it = state->eager_push_peers->head; it != NULL; it = it->next) {
        peer* p = (peer*)it->data;
        if(!sender || !equal_peer(p, sender)) {
            list_add_item_to_tail(state->missingAcks, p);
            send_gossip_msg(p, state->self, requester_id, mid->hash, req, state->proto_id);
        }
    }

    if(state->missingAcks->size == 0) {
        pipelined_msg* pipelined = list_remove_head(state->pipeline);
        if(pipelined) {
            state->ongoing = pipelined->mid;
            request_msg_body(state->ongoing, &pipelined->ip, pipelined->isGraft, state->proto_id);
        } else
            state->ongoing = NULL;
    }
}

static void lazy_push(int mid, peer* sender, plumtree_state* state) {
    for(list_item* it = state->lazy_push_peers->head; it != NULL; it = it->next) {
        peer* p = (peer*)it->data;
        if(!sender || !equal_peer(p, sender)) {
            lazy_queue_item* lqi = create_lazy_queue_item(p, mid);
            list_add_item_to_tail(state->lazy_queue, lqi);
        }
    }

    dispatch_announcements(state);
}

static void process_gossip(YggMessage* msg, void* ptr, plumtree_state* state) {


    uint32_t mid_t;
    ptr = YggMessage_readPayload(msg, ptr, &mid_t, sizeof(uint32_t));
    int mid = ntohl(mid_t);

#if defined DEBUG | DEBUG_PLUMTREE
    char debug_msg[100];
    bzero(debug_msg, 100);
    sprintf(debug_msg, "processing gossip %d from %s %d", mid, msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
    ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

    message_item* m = NULL;
    if((m = list_find_item(state->received, (equal_function) equal_message_item, &mid)) == NULL) {
        uint16_t req_id_t;
        uint16_t h_size;
        uint16_t b_size;

        ptr = YggMessage_readPayload(msg, ptr, &req_id_t, sizeof(uint16_t));

        ptr = YggMessage_readPayload(msg, ptr, &h_size, sizeof(uint16_t));
        ptr = YggMessage_readPayload(msg, ptr, &b_size, sizeof(uint16_t));

        unsigned short req_id = ntohs(req_id_t);

        dissemination_request req;
        req.header_size = ntohs(h_size);
        req.body_size = ntohs(b_size);

        req.header = ptr;
        if(req.body_size > 0)
            req.body = ptr+req.header_size;
        else
            req.body = NULL;

        deliver_msg(req_id, ptr, req.header_size+req.body_size, &state->self->ip, &msg->header.src_addr.ip);

        m = create_msg_item(mid, req_id, ptr, req.header_size);
        list_add_item_to_head(state->received, m);

        lazy_queue_item* lqi = NULL;
        bool cancel = false;
        while((lqi = list_remove_item(state->missing, (equal_function) equal_mid, &mid)) != NULL) {
            free(lqi);
            cancel = true;
        }
        if(cancel)
            cancel_timer(mid, state);

        peer* p = list_find_item(state->eager_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
        if(!p)
            p = list_find_item(state->lazy_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);

        if(!p) {
            char warning_msg[100];
            bzero(warning_msg, 100);
            sprintf(warning_msg, "processing new message, but not know the peer %s  %d, being optimistic\n", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
            ygg_log("PLUMTREE", "WARNING", warning_msg);
        }

        if(p) {
            send_gossip_ack(p, state->self, state->proto_id);
        } else {
            p = create_empty_peer();
            p->ip = msg->header.src_addr.ip;
            send_gossip_ack(p, state->self, state->proto_id);
            free(p);
        }

        if(!state->ongoing) {
            eager_push(&req, req_id, m, p, state);
            lazy_push(mid, p, state);
        } else {
            add_to_pipeline(state->pipeline, m, &msg->header.src_addr.ip, false);
        }

        //TODO: optimization


    }else {
        //printf("already seen this message mid %d\n", m->hash);
        peer* p = list_remove_item(state->eager_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
        if(p) {
            list_add_item_to_tail(state->lazy_push_peers, p);
        } else
            p = list_find_item(state->lazy_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);

        if(p) {
            send_prune_msg(p, state->self, state->proto_id);
            remove_from_pipeline(state->pipeline, m, &p->ip, true);
        } else {
            char warning_msg[100];
            bzero(warning_msg, 100);
            sprintf(warning_msg, "I do not know node %s  %d to send prune message, ignoring\n", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
            ygg_log("PLUMTREE", "WARNING", warning_msg);
            p = create_empty_peer();
            p->ip = msg->header.src_addr.ip;
            send_prune_msg(p, state->self, state->proto_id);
            free(p);
        }

    }

    YggMessage_freePayload(msg);

}

static void process_gossip_ack(YggMessage* msg, void* ptr, plumtree_state* state) {

#if defined DEBUG | DEBUG_PLUMTREE
    char debug_msg[100];
    bzero(debug_msg, 100);
    sprintf(debug_msg, "processing gossip ack from %s %d", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
    ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

    list_remove_item(state->missingAcks, (equal_function)equal_peer_addr,  &msg->header.src_addr.ip);
    if(state->missingAcks->size == 0) {
        pipelined_msg* pipelined = list_remove_head(state->pipeline);
        if(pipelined) {
            state->ongoing = pipelined->mid;
            request_msg_body(state->ongoing, &pipelined->ip, pipelined->isGraft, state->proto_id);
        } else
            state->ongoing = NULL;
    }

    YggMessage_freePayload(msg);
}

static void process_prune(YggMessage* msg, void* ptr, plumtree_state* state) {

#if defined DEBUG | DEBUG_PLUMTREE
    char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing prune from %s %d", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

    peer* p = list_remove_item(state->eager_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
    if(p)
        list_add_item_to_tail(state->lazy_push_peers, p);

    list_remove_item(state->missingAcks, (equal_function)equal_peer_addr,  &msg->header.src_addr.ip);
    if(state->missingAcks->size == 0) {
        pipelined_msg* pipelined = list_remove_head(state->pipeline);
        if(pipelined) {
            state->ongoing = pipelined->mid;
            request_msg_body(state->ongoing, &pipelined->ip, pipelined->isGraft, state->proto_id);
        } else
            state->ongoing = NULL;
    }

    YggMessage_freePayload(msg);
}

static void process_ihave(int mid, peer* sender, plumtree_state* state) {

    if(!list_find_item(state->received, (equal_function) equal_message_item, &mid)) {
        if(!list_find_item(state->timers, (equal_function) equal_timer, &mid)) {
            set_timer(mid, state);
        }
        lazy_queue_item* lqi = create_lazy_queue_item(sender, mid);
        list_add_item_to_tail(state->missing, lqi);
    }

    message_item i;
    i.hash = mid;
    remove_from_pipeline(state->pipeline, &i, &sender->ip, true);
}

static void process_ihave_msg(YggMessage* msg, void* ptr, plumtree_state* state) {

    uint32_t mid_t;

    ptr = YggMessage_readPayload(msg, ptr, &mid_t, sizeof(uint32_t));

    int mid = ntohl(mid_t);

#if defined DEBUG | DEBUG_PLUMTREE
    char debug_msg[100];
    bzero(debug_msg, 100);
    sprintf(debug_msg, "processing ihave %d from %s %d", mid, msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
    ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

    peer* p = list_find_item(state->lazy_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
    if(!p)
        p = list_find_item(state->eager_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);

    if(!p) {
        char warning_msg[100];
        bzero(warning_msg, 100);
        sprintf(warning_msg, "I do not know this peer %s  %d  on receive Ihave msg, ignoring\n", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
        ygg_log("PLUMTREE", "WARNING", warning_msg);
    }
    else
        process_ihave(mid, p, state);
    YggMessage_freePayload(msg);

}

static void process_announcement(YggMessage* msg, void* ptr, plumtree_state* state) {
    //do nothing for now.
    YggMessage_freePayload(msg);
}


static void process_graft(YggMessage* msg, void* ptr, plumtree_state* state) {

    uint32_t mid_t;

    ptr = YggMessage_readPayload(msg, ptr, &mid_t, sizeof(uint32_t));

    int mid = ntohl(mid_t);

#if defined DEBUG | DEBUG_PLUMTREE
    char debug_msg[100];
    bzero(debug_msg, 100);
    sprintf(debug_msg, "processing graft %d from %s %d", mid, msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
    ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

    peer* p = list_remove_item(state->lazy_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
    if(p) {
        list_add_item_to_tail(state->eager_push_peers, p);
    } else
        p = list_find_item(state->eager_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
    message_item* m = NULL;
    if(!p) {
        char warning_msg[100];
        bzero(warning_msg, 100);
        sprintf(warning_msg, "I do not known graft message originator  %s %d, ignoring", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
        ygg_log("PLUMTREE", "WARNING", warning_msg);
    }
    else if((m = list_find_item(state->received, (equal_function) equal_message_item, &mid)) != NULL) {
        if(!state->ongoing) {
            state->ongoing = m;
            request_msg_body(m, &p->ip, true, state->proto_id);
        } else {
            add_to_pipeline(state->pipeline, m, &p->ip, true);
        }
    }

    YggMessage_freePayload(msg);
}


static void process_msg(YggMessage* msg, plumtree_state* state) {

    uint16_t type;
    void* ptr = YggMessage_readPayload(msg, NULL, &type, sizeof(uint16_t));
    plumtree_msg_types msg_type = decodify(type);

    switch(msg_type) {
        case GOSSIP:
            process_gossip(msg, ptr, state);
            break;
        case PRUNE:
            process_prune(msg, ptr, state);
            break;
        case IHAVE:
            process_ihave_msg(msg, ptr, state);
            break;
        case GRAFT:
            process_graft(msg, ptr, state);
            break;
        case ANNOUNCEMENT: //these message are not yet processed
            process_announcement(msg, ptr, state);
            break;
        case GOSSIP_ACK:
            process_gossip_ack(msg, ptr, state);
            break;
    }

}

static void garbage_collect(plumtree_state* state) {

#ifdef DEBUG
    print_tree(state);
#endif

    list_item* it = state->received->head;
    list_item* prev = NULL;
    while(it != NULL) {
        bool destroy = false;
        message_item* m = (message_item*)it->data;

        if(expired(m)) {
            destroy = true;
        }else {
            prev = it;
        }
        it = it->next;
        if(destroy) {
            if(prev) {
                m = list_remove(state->received, prev);
            } else
                m = list_remove_head(state->received);

            destroy_message_item(m);
            free(m);
        }

    }

}

static void process_timer(YggTimer* timer, plumtree_state* state) {

    if(timer->timer_type == 2)
        print_tree(state);
    else if(timer->timer_type == 1)
        garbage_collect(state);
    else {

        int mid;
        YggTimer_readPayload(timer, NULL, &mid, sizeof(int));

#if defined DEBUG | DEBUG_PLUMTREE
        char debug_msg[100];
		bzero(debug_msg, 100);
		sprintf(debug_msg, "received timeout for mid: %d", mid);
		ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

        lazy_queue_item* lqi = list_remove_item(state->missing, (equal_function) equal_mid, &mid);
        if (!lqi) {
            char warning_msg[100];
            bzero(warning_msg, 100);
            sprintf(warning_msg, "no item missing for mid: %d", mid);
            ygg_log("PLUMTREE", "WARNING", warning_msg);
            YggTimer_freePayload(timer);
            return;
        }

        peer *p = list_remove_item(state->lazy_push_peers, (equal_function) equal_peer, lqi->node);
        if (p)
            list_add_item_to_tail(state->eager_push_peers, p);
        else
            p = list_find_item(state->eager_push_peers, (equal_function) equal_peer, lqi->node);

        if (p) {
            send_graft_msg(p, state->self, mid, state->proto_id);
        } else {
            char warning_msg[100];
            bzero(warning_msg, 100);
            sprintf(warning_msg, "no peer to send graft missing msg mid: %d", mid);
            ygg_log("PLUMTREE", "WARNING", warning_msg);
        }


        if (list_find_item(state->timers, (equal_function) equal_timer, &mid)) {
            YggTimer_set(timer, state->timeout_s,
                        state->timeout_ns, 0,
                         0); //TODO: better set timeout 2: to an average roundtrip time

            //printf("setting up timer for %d timeout: %d %ld\n", mid, timer->config.first_notification.tv_sec, timer->config.first_notification.tv_nsec);
            setupTimer(timer);
        }

        YggTimer_freePayload(timer);
    }

}

static void process_neighbour_up(YggEvent* ev, plumtree_state* state) {
    peer* p = create_empty_peer();
    void* ptr = YggEvent_readPayload(ev, NULL, p->ip.addr, 16);
    YggEvent_readPayload(ev, ptr, &p->ip.port, sizeof(unsigned short));

    list_add_item_to_tail(state->eager_push_peers, p);
}

static void process_neighbour_down(YggEvent* ev, plumtree_state* state) {
    IPAddr ip;
    void* ptr = YggEvent_readPayload(ev, NULL, ip.addr, 16);
    YggEvent_readPayload(ev, ptr, &ip.port, sizeof(unsigned short));

    peer* p = NULL;
    p = list_remove_item(state->eager_push_peers, (equal_function) equal_peer_addr, &ip);

    peer* c = list_remove_item(state->lazy_push_peers, (equal_function) equal_peer_addr, &ip);
    if(c)
        p = c;

    pipelined_msg* pipelined = NULL;
    while((pipelined = list_remove_item(state->pipeline, (equal_function) equal_graft_request_sender, &p->ip))) {
        free(pipelined);
    }

    lazy_queue_item* lqi = NULL;
    while((lqi = list_remove_item(state->missing, (equal_function) equal_node, p)) != NULL) {
        free(lqi);
    }

    while(list_remove_item(state->lazy_queue, (equal_function) equal_node, p));

    c = list_remove_item(state->missingAcks, (equal_function) equal_peer, p);
    if(c && state->missingAcks->size == 0) {
        pipelined = list_remove_head(state->pipeline);
        if(pipelined) {
            state->ongoing = pipelined->mid;
            request_msg_body(state->ongoing, &pipelined->ip, pipelined->isGraft, state->proto_id);
        } else
            state->ongoing = NULL;
    }

    destroy_peer(p);

}

static void process_event(YggEvent* ev, plumtree_state* state) {
    switch(ev->notification_id) {
        case OVERLAY_NEIGHBOUR_UP:
            process_neighbour_up(ev, state);
            break;
        case OVERLAY_NEIGHBOUR_DOWN:
            process_neighbour_down(ev, state);
            break;
    }

    YggEvent_freePayload(ev);
}

static void broadcast(short proto_origin, dissemination_request* req, plumtree_state* state) {

    int mid = generate_mid(req->header, req->header_size, proto_origin, state->self);



    void* msg_payload = malloc(req->header_size+req->body_size);
    memcpy(msg_payload, req->header, req->header_size);
    if(req->body)
        memcpy(msg_payload+req->header_size, req->body, req->body_size);

    //this is to maintain broadcast semantics
#ifdef DEBUG
    char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg,"broadcasting msg: mid: %d", mid);
	ygg_log("PLUMTREE", "DEBUG", debug_msg);


#endif
    deliver_msg(proto_origin, msg_payload, req->header_size+req->body_size, &state->self->ip, &state->self->ip);

    message_item* m = create_msg_item(mid, proto_origin, req->header, req->header_size);

    list_add_item_to_tail(state->received, m);

    if(!state->ongoing) {
        eager_push(req, proto_origin, m, state->self, state);
        lazy_push(mid, state->self, state);
    } else {
        //printf("added %d from %s %d isgraft %s to pipeline\n", m->hash, state->self->ip.addr, state->)
        add_to_pipeline(state->pipeline, m, &state->self->ip, false);
    }


    free(msg_payload);
    free(req->header);
    if(req->body)
        free(req->body);

}


static void process_request(YggRequest* req, plumtree_state* state) {

    if(req->request == REQUEST && req->request_type == DISSEMINATION_REQUEST) {
        broadcast(req->proto_origin, (dissemination_request*)req->payload, state);
        YggRequest_freePayload(req);
    } else if(req->request == REPLY && req->request_type == MSG_BODY_REQ) {
        dissemination_msg_request* msg = req->payload;
        plumtree_request_msg_body_header* header = (plumtree_request_msg_body_header*) msg->header;
        peer p;
        p.ip = header->ip;
        //printf("msg body size: %d\n", msg->req->body_size);

        if(!header->isGraft) {
            message_item* it = list_find_item(state->received, (equal_function) equal_message_item, &header->mid);
            eager_push(msg->req, req->proto_origin, it, &p, state);
            lazy_push(header->mid , &p, state);
        } else {
            send_gossip_msg(&p, state->self, req->proto_origin, header->mid, msg->req,
                            state->proto_id);
            peer* s = list_find_item(state->eager_push_peers, (equal_function) equal_peer, &p);
            if(!s)
                s = list_find_item(state->lazy_push_peers, (equal_function) equal_peer, &p);

            if(s)
                list_add_item_to_tail(state->missingAcks, s);
            else {
                ygg_log("PLUMTREE", "WARNING",  "PIPELINED MESSAGE DOES NOT HAVE A DESTINATION!!!!!");
            }
        }

        free(msg->req->header);
        if (msg->req->body)
            free(msg->req->body);
        free(msg->req);
        free(msg->header);
        YggRequest_freePayload(req);

    } else if(req->request == REQUEST && req->proto_origin == PROTO_DISPATCH && req->request_type == ASYNC_MSG_SENT) {
        //TODO
//        IPAddr ip;
//        void* ptr = YggRequest_readPayload(req, NULL, ip.addr, 16);
//        YggRequest_readPayload(req, ptr, &ip.port, sizeof(unsigned short));

        //handle_next_msg(req, state);
    }
}

static void plumtree_main_loop(main_loop_args* args) {

    queue_t* inBox = args->inBox;
    plumtree_state* state = args->state;

    while(true) {
        queue_t_elem el;
        queue_pop(inBox, &el);

        switch(el.type) {
            case YGG_MESSAGE:
                process_msg(&el.data.msg, state);
                break;
            case YGG_TIMER:
                process_timer(&el.data.timer, state);
                break;
            case YGG_EVENT:
                process_event(&el.data.event, state);
                break;
            case YGG_REQUEST:
                process_request(&el.data.request, state);
                break;
        }
    }

}


proto_def* plumtree_mem_optimized_flow_control_init(plumtree_flow_control_args* args) {

    plumtree_state* state = malloc(sizeof(plumtree_state));

    state->received = list_init();
    state->eager_push_peers = list_init();
    state->lazy_push_peers = list_init();
    state->missing = list_init();
    state->lazy_queue = list_init();

    state->timers = list_init();

    state->timeout_s = args->plumtreeArgs->timeout_s;
    state->timeout_ns = args->plumtreeArgs->timeout_ns;
    state->fanout = args->plumtreeArgs->fanout;

    state->self = create_empty_peer();
    getIpAddr(&state->self->ip);

    state->proto_id = PROTO_PLUMTREE;

    state->dispatch_msg_size_threashold = args->dispatcher_msg_size_threashold;

    state->ongoing = NULL;
    state->missingAcks = list_init();
    state->pipeline = list_init();

    proto_def* plumtree = create_protocol_definition(PROTO_PLUMTREE, "PlumTree", state, NULL);
    proto_def_add_consumed_event(plumtree, args->plumtreeArgs->membership_id, OVERLAY_NEIGHBOUR_UP);
    proto_def_add_consumed_event(plumtree, args->plumtreeArgs->membership_id, OVERLAY_NEIGHBOUR_DOWN);

    proto_def_add_protocol_main_loop(plumtree, (Proto_main_loop) plumtree_main_loop);

    YggTimer gc;
    YggTimer_init(&gc, PROTO_PLUMTREE, PROTO_PLUMTREE);
    YggTimer_set(&gc, GC_TIME_S, 0, GC_TIME_S, 0);
    YggTimer_setType(&gc, 1); //lets say 1 == garbage collect

    setupTimer(&gc);

#ifdef DEBUG
    YggTimer debug;
    YggTimer_init(&debug, PROTO_PLUMTREE, PROTO_PLUMTREE);
    YggTimer_set(&debug, 5, 0, 5, 0);
    YggTimer_setType(&debug, 2); //lets say 2 == debug (print tables)

    setupTimer(&debug);
#endif
    return plumtree;

}


plumtree_flow_control_args* plumtree_flow_control_args_init(int fanout, unsigned short timeout_s, long timeout_ns, unsigned short membership_id, int dispatcher_msg_size_threashold) {
    plumtree_flow_control_args* args = malloc(sizeof(plumtree_flow_control_args));
    args->plumtreeArgs = plumtree_args_init(fanout, timeout_s, timeout_ns, membership_id);
    args->dispatcher_msg_size_threashold = dispatcher_msg_size_threashold;
    return args;
}
void plumtree_flow_control_args_destroy(plumtree_flow_control_args* args) {
    plumtree_args_destroy(args->plumtreeArgs);
    free(args);

}