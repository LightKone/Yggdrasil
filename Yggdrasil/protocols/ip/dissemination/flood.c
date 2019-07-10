//
// Created by Pedro Akos on 2019-07-01.
//

#include "flood.h"

typedef struct __peer {
    IPAddr ip;
}peer;

static bool equal_peer_addr(peer* p, IPAddr* ip) {
    return  (strcmp(p->ip.addr, ip->addr) == 0 && p->ip.port == ip->port);
}

static bool equal_peer(peer* p1, peer* p2) {
    return (strcmp(p1->ip.addr, p2->ip.addr) == 0 && p1->ip.port == p2->ip.port);
}

#define GC_TIME_S 600 //maybe this should be a parameter

typedef enum __flood_msg_types {
    GOSSIP,
}flood_msg_types;

static flood_msg_types decodify(uint16_t type) {
    return ntohs(type);
}

static uint16_t codify(flood_msg_types type) {
    return htons(type);
}

static void init_msg_header(YggMessage* msg, flood_msg_types type, peer* dest, unsigned proto_id) {
    YggMessage_initIp(msg, proto_id, dest->ip.addr, dest->ip.port);
    uint16_t msg_type = codify(type);
    YggMessage_addPayload(msg, (char*) &msg_type, sizeof(uint16_t));
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

    free(to_hash);

    return mid;
}


typedef struct __mid {
    int hash;
    struct timespec received;
}message_item;

static bool equal_message_item(message_item* m, int* mid){
    return m->hash == *mid;
}

static message_item* create_msg_item(int mid) {
    message_item* m = malloc(sizeof(message_item));
    m->hash = mid;
    clock_gettime(CLOCK_MONOTONIC, &m->received);
    return m;
}

static void destroy_message_item(message_item* m) {
    free(m);
}

static bool expired(message_item* m) { //TODO better this
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    return (now.tv_sec - m->received.tv_sec > GC_TIME_S);
}


typedef struct __flood_state {

    unsigned short proto_id;
    peer* self;

    list* peers;

    list* received;

}flood_state;

static void deliver_msg(unsigned short proto_dest, void* msg_contents, unsigned short msg_size, IPAddr* dest_ip, IPAddr* src_ip) {

    YggMessage m;
    YggMessage_initIp(&m, proto_dest, dest_ip->addr, dest_ip->port);
    memcpy(m.header.src_addr.ip.addr, src_ip->addr, strlen(src_ip->addr));
    m.header.src_addr.ip.port = src_ip->port;
    m.data = msg_contents;
    m.dataLen = msg_size;

    deliver(&m);
}

static void send_gossip_msg(peer* dest, peer* self, unsigned short requester_id, int mid, dissemination_request* req, unsigned short proto_id) {

#ifdef DEBUG
    char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending gossip message with mid: %d to: %s %d\n", mid, dest->ip.addr, dest->ip.port);
	ygg_log("FLOOD", "DEBUG", debug_msg);
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

static void flood(dissemination_request* req, unsigned short requester_id, int mid, peer* sender, flood_state* state) {
    for(list_item* it = state->peers->head; it != NULL; it = it->next) {
        peer* p = (peer*)it->data;
        if(!sender)
            send_gossip_msg(p, state->self, requester_id, mid, req, state->proto_id);
        else if(!equal_peer(p, sender))
            send_gossip_msg(p, state->self, requester_id, mid, req, state->proto_id);
    }
}

static void process_gossip(YggMessage* msg, void* ptr, flood_state* state) {
#ifdef DEBUG
    char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing gossip from %s %d", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("FLOOD", "DEBUG", debug_msg);
#endif

    uint32_t mid_t;
    ptr = YggMessage_readPayload(msg, ptr, &mid_t, sizeof(uint32_t));
    int mid = ntohl(mid_t);
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

        m = create_msg_item(mid);
        list_add_item_to_head(state->received, m);

        peer* p = list_find_item(state->peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);

        if(!p) {
            char warning_msg[100];
            bzero(warning_msg, 100);
            sprintf(warning_msg, "processing new message, but not know the peer %s  %d, being optimistic\n", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
            ygg_log("FLOOD", "WARNING", warning_msg);
        }
        flood(&req, req_id, mid, p, state);


        //TODO: optimization


    }else {
        //do nothing
    }

    YggMessage_freePayload(msg);
}

static void process_msg(YggMessage* msg, flood_state* state) {
    uint16_t type;
    void* ptr = YggMessage_readPayload(msg, NULL, &type, sizeof(uint16_t));
    flood_msg_types msg_type = decodify(type);

    switch(msg_type) {
        case GOSSIP:
            process_gossip(msg, ptr, state);
            break;
    }
}

static void process_event(YggEvent* ev, flood_state* state) {

    if(ev->notification_id == OVERLAY_NEIGHBOUR_UP) {
        peer* p = malloc(sizeof(peer));
        void* ptr = YggEvent_readPayload(ev, NULL, p->ip.addr, 16);
        YggEvent_readPayload(ev, ptr, &p->ip.port, sizeof(unsigned short));
        list_add_item_to_tail(state->peers, p);
    } else if (ev->notification_id == OVERLAY_NEIGHBOUR_DOWN) {
        IPAddr ip;
        void* ptr = YggEvent_readPayload(ev, NULL, ip.addr, 16);
        YggEvent_readPayload(ev, ptr, &ip.port, sizeof(unsigned  short));
        peer* p = list_remove_item(state->peers, (equal_function) equal_peer_addr, &ip);
        free(p);
    } else {
        //ignore
    }

    YggEvent_freePayload(ev);

}

static void broadcast(short proto_origin, dissemination_request* req, flood_state* state) {

    int mid = generate_mid(req->header, req->header_size, proto_origin, state->self);

    void* msg_payload = malloc(req->header_size+req->body_size);
    memcpy(msg_payload, req->header, req->header_size);
    if(req->body)
        memcpy(msg_payload+req->header_size, req->body, req->body_size);

    flood(req, proto_origin, mid, state->self, state);

    //this is to maintain broadcast semantics
#ifdef DEBUG
    char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg,"broadcasting msg: mid: %d round: %d", mid, 0);
	ygg_log("FLOOD", "DEBUG", debug_msg);


#endif
    deliver_msg(proto_origin, msg_payload, req->header_size+req->body_size, &state->self->ip, &state->self->ip);

    message_item* m = create_msg_item(mid);

    list_add_item_to_tail(state->received, m);

    free(msg_payload);
    free(req->header);
    if(req->body)
        free(req->body);

}

static void process_request(YggRequest* req, flood_state* state) {

    if(req->request_type == DISSEMINATION_REQUEST) {
        broadcast(req->proto_origin, (dissemination_request*)req->payload, state);
    } else if(req->request_type == MSG_BODY_REQ && req->request == REPLY) {
        //does not request the message body
    }

}

static void process_timer(YggTimer* timer, flood_state* state) {
    //only use to garbage collect
}

static void flood_main_loop(main_loop_args* args) {
    queue_t* inBox = args->inBox;
    flood_state* state = args->state;

    while (1) {
        queue_t_elem elem;
        queue_pop(inBox, &elem);
        switch (elem.type) {
            case YGG_MESSAGE:
                process_msg(&elem.data.msg, state);
                break;
            case YGG_EVENT:
                process_event(&elem.data.event, state);
                break;
            case YGG_REQUEST:
                process_request(&elem.data.request, state);
            case YGG_TIMER:
                process_timer(&elem.data.timer, state);
                break;
        }
    }
}


proto_def* flood_init(flood_args* args) {

    flood_state* state = malloc(sizeof(flood_state));

    state->peers = list_init();
    state->received = list_init();

    state->self = malloc(sizeof(peer));
    getIpAddr(&state->self->ip);

    state->proto_id = PROTO_FLOOD;

    proto_def* flood = create_protocol_definition(PROTO_FLOOD, "flood dissemination", state, NULL);

    proto_def_add_protocol_main_loop(flood, (Proto_main_loop) flood_main_loop);

    proto_def_add_consumed_event(flood, PROTO_HYPARVIEW, OVERLAY_NEIGHBOUR_UP);
    proto_def_add_consumed_event(flood, PROTO_HYPARVIEW, OVERLAY_NEIGHBOUR_DOWN);


    return flood;
}

flood_args* flood_args_init() {
    flood_args* args = malloc(sizeof(flood_args));
    return args;
}
void flood_args_destroy(flood_args* args) {
    free(args);
}