//
// Created by Pedro Akos on 2019-06-07.
//

#include "block_data_transfer_mem_opt.h"


static int hash(char* to_hash, unsigned short to_hash_len) {
    return DJBHash(to_hash, to_hash_len);
}

typedef struct _on_going_transfer{
    int filename_hash;
    int version;
    int creator;

    int n_blocks;
    uint8_t* blocks;

    time_t start;
}on_going_transfer;


static bool is_marked(uint8_t* blocks, int block) {
    int k = block/8;
    int pos = block%8;

    int flag = 1;
    flag = flag << (7-pos);

    return blocks[k] & flag;
}


static void mark_block(uint8_t* blocks, int block) {
    int k = block/8;
    int pos = block%8;

    int flag = 1;
    flag = flag << (7-pos);

    blocks[k] |= flag;
}

static int next_missing_block(uint8_t* blocks, int n_blocks) {

    unsigned short bytes = n_blocks%8 == 0 ? n_blocks/8 : (n_blocks/8) +1;
    for(int i = 0; i < bytes; i++) {
        if((~blocks[i])) {
            unsigned  short off = i == bytes-1 ? n_blocks%8 : 8;
            off = off == 0 ? 8 : off;
            for(int k = 7; k >= (8-off); k--){
                if(!((blocks[i]>>k)&1))
                    return (i*8)+(8-(k+1));
            }
        }
    }

    return -1;
}

static int total_missing_blocks(uint8_t* blocks, int n_blocks) {
    unsigned short bytes = n_blocks%8 == 0 ? n_blocks/8 : (n_blocks/8) +1;
    int total = 0;
    for(int i = 0; i < bytes; i++) {
        if((~blocks[i])) {
            unsigned  short off = i == bytes-1 ? n_blocks%8 : 8;
            off = off == 0 ? 8 : off;
            for(int k = 7; k >= (8-off); k--){
                if(!((blocks[i]>>k)&1))
                    total+=1;
            }
        } else
            total +=8;
    }

    return total;
}

static bool equal_file(on_going_transfer* t, char* filename) {
    return t->filename_hash == hash(filename, strlen(filename));
}

typedef struct __block_data_transfer_state {

    short my_id;
    short dissemination_proto;
    short dissemination_request;

    off_t block_size;

    //local test dir
    char* dir;
    int dir_fd;

    char* staged_area;

    int id;

    list* on_going_reception;
    list* on_going_transmission;

}block_data_transfer_state;

typedef enum _block_data_transfer_msgs {
    BROADCAST_FILE,
    DIRECT_FILE,
    HANDSHAKE,
    HANDSHAKE_REPLY
}block_data_transfer_msgs;

static uint16_t codify(block_data_transfer_msgs msg_type) {
    return htons(msg_type);
}

static block_data_transfer_msgs decodify(uint16_t code) {
    return ntohs(code);
}

typedef struct file_meta_data {
    int fd;
    char* filename;
    off_t file_size;
    mode_t mode;

    int version;
    int creator;

    int block;
    int n_blocks;
    int block_size;
}file_meta;

static void write_new_version(file_meta* meta, block_data_transfer_state* state) {

    bool new_file = true;
    int filename_hash = hash(meta->filename, strlen(meta->filename));
    int hash;

    while(read(state->dir_fd, &hash, sizeof(int)) > 0) {

        if(hash == filename_hash) {
            write(state->dir_fd, &meta->version, sizeof(int));
            write(state->dir_fd, &meta->creator, sizeof(int));
            new_file = false;
            break;
        } else
            lseek(state->dir_fd, sizeof(int)*2, SEEK_CUR);

    }

    if(new_file) {
        write(state->dir_fd, &filename_hash, sizeof(int));
        write(state->dir_fd, &meta->version, sizeof(int));
        write(state->dir_fd, &meta->creator, sizeof(int));
    }


    lseek(state->dir_fd, 0, SEEK_SET);
}

static int inc_version(file_meta* meta, block_data_transfer_state* state) {

    int version = -1;
    int filename_hash = hash(meta->filename, strlen(meta->filename));
    int hash;
    int pos = 0;
    while(read(state->dir_fd, &hash, sizeof(int)) > 0) {
        pos += sizeof(int);
        if(hash == filename_hash) {
            read(state->dir_fd, &version, sizeof(int));

            version ++;
            lseek(state->dir_fd, pos, SEEK_SET);
            write(state->dir_fd, &version, sizeof(int));
            write(state->dir_fd, &state->id, sizeof(int));
            break;
        } else {
            lseek(state->dir_fd, sizeof(int)*2, SEEK_CUR);
            pos += sizeof(int)*2;
        }

    }

    if(version < 0) {
        version = 0;
        write(state->dir_fd, &filename_hash, sizeof(int));
        write(state->dir_fd, &version, sizeof(int));
        write(state->dir_fd, &state->id, sizeof(int));
    }


    lseek(state->dir_fd, 0, SEEK_SET);
    return version;
}

static bool is_new_version(file_meta* meta, block_data_transfer_state* state) {
    bool new_version = true;
    int filename_hash = hash(meta->filename, strlen(meta->filename));
    int hash;
    while(read(state->dir_fd, &hash, sizeof(int)) > 0) {

        if(hash == filename_hash) {
            int version;
            read(state->dir_fd, &version, sizeof(int));
            if(version == meta->version) {
                int creator;
                read(state->dir_fd, &creator, sizeof(int));
                new_version = meta->creator > creator;
            } else
                new_version = meta->version > version;

            break;
        }
        lseek(state->dir_fd, sizeof(int)*2, SEEK_CUR);
    }

    lseek(state->dir_fd, 0, SEEK_SET);
    return new_version;
}

static bool is_same_version(file_meta* meta, block_data_transfer_state* state) {
    bool new_version = false;
    int filename_hash = hash(meta->filename, strlen(meta->filename));
    int hash;
    while(read(state->dir_fd, &hash, sizeof(int)) > 0) {

        if(hash == filename_hash) {
            int version;
            read(state->dir_fd, &version, sizeof(int));
            if(version == meta->version) {
                int creator;
                read(state->dir_fd, &creator, sizeof(int));
                new_version = meta->creator == creator;
            }

            break;
        }
        lseek(state->dir_fd, sizeof(int)*2, SEEK_CUR);
    }

    lseek(state->dir_fd, 0, SEEK_SET);
    return new_version;
}

static on_going_transfer* create_on_going_transfer(file_meta* meta, int filename_size, int blocks, list* transmission_list) {
    on_going_transfer* t = malloc(sizeof(on_going_transfer));
    t->filename_hash = hash(meta->filename, filename_size);

    t->n_blocks = blocks;
    t->blocks = malloc(t->n_blocks);
    bzero(t->blocks, t->n_blocks);

    t->version = meta->version;
    t->creator = meta->creator;

    t->start = time(NULL);

    list_add_item_to_head(transmission_list, t);

    return t;
}

static void comple_transfer(file_meta* meta, list* transmission_list, block_data_transfer_state* state) {
    on_going_transfer* t = list_remove_item(transmission_list, (equal_function) equal_file, meta->filename);
    free(t->blocks);
    free(t);

    write_new_version(meta, state);

    //mv_from_staged(meta, state);
}



static file_meta* process_file_meta(int fd, char* filename, block_data_transfer_state* state){
    struct stat s;
    if(fstat(fd, &s) < 0){
        perror("FSTAT: ");
        return NULL;
    }

    file_meta* meta = malloc(sizeof(file_meta));
    meta->fd = fd;
    meta->filename = filename;
    meta->file_size = s.st_size;
    meta->mode = s.st_mode;

    meta->version = inc_version(meta, state);
    meta->creator = state->id;

    return meta;
}

static char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

static char* change_file_location(char* filename, block_data_transfer_state* state) {

//    char* str = strtok(filename,"/");;
//    char file[200];
//    while(str != NULL) {
//        bzero(file, 200);
//        memcpy(file, str, strlen(str));
//        str = strtok(NULL,"/");
//    }
    char* f = concat("/", filename);

    char* location = concat(state->dir, f);

    free(f);
    return location;
}

static char* to_staged(char* filename, block_data_transfer_state* state) {

//    char* str = strtok(filename,"/");;
//    char file[200];
//    while(str != NULL) {
//        bzero(file, 200);
//        memcpy(file, str, strlen(str));
//        str = strtok(NULL,"/");
//    }
    char* f = concat("/", filename);

    char* location = concat(state->staged_area, f);

    free(f);
    return location;
}

static int open_file(char* filename) {
    int fd = open(filename, O_RDONLY);
    return fd;
}

static bool read_file_to_mem(void* buffer, int block_size, file_meta* meta, int off, block_data_transfer_state* state) {


    void* ptr;
    if((ptr = mmap(NULL, block_size, PROT_READ, MAP_PRIVATE, meta->fd, state->block_size*off)) != MAP_FAILED) {
        memcpy(buffer, ptr, block_size);
        munmap(ptr, block_size);

        return true;
    }

    return false;

}

static bool write_file_to_disk(void* buffer, file_meta* meta, char* location, bool new_version) {


    if(new_version)
        meta->fd = open(location, O_CREAT | O_RDWR | O_TRUNC, meta->mode);
    else
        meta->fd = open(location, O_CREAT | O_RDWR, meta->mode);

    off_t block_size = meta->block_size;
    if(meta->block > 0 && meta->block == meta->n_blocks-1 && meta->block_size != (meta->file_size/meta->n_blocks))
        block_size = (meta->file_size-meta->block_size)/(meta->n_blocks-1);


    //printf("Seek file to off_set %ld\n", block_size*meta->block);
    lseek(meta->fd, block_size*meta->block, SEEK_SET);

    //printf("Writing block %d out of %d of file %s version %d:%d to disk\n", meta->block, meta->n_blocks, meta->filename, meta->version, meta->creator);
    off_t writen = 0;
    int n;
    while(writen < meta->block_size) {
        if((n = write(meta->fd, buffer + writen, meta->block_size - writen)) < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            perror("Error on writing file: ");
            break;
        }

        writen += n;
        //printf("Written %ld out of %d\n", writen, meta->block_size);
    }


    close(meta->fd);

//    ftruncate(meta->fd, meta->file_size+1);
//
//    void* ptr;
//    if((ptr = mmap(NULL, meta->file_size+1,  PROT_READ | PROT_WRITE, MAP_SHARED, meta->fd, 0))) {
//        memcpy(ptr, buffer, meta->file_size);
//        //*((char*)(ptr + meta->file_size) ) = '\0';  //write a 0 at the end of file
//
//        msync(ptr, meta->file_size+1, MS_SYNC);
//        munmap(ptr, meta->file_size+1);
//        return true;
//    }
//
//    return false;
    return true;
}

static void request_block_dissemination(block_data_transfer_state* state, file_meta* meta, int i, int blocks) {

    YggRequest req;
    YggRequest_init(&req, state->my_id, state->dissemination_proto, REQUEST, state->dissemination_request);

    dissemination_request* d_req = dissemination_request_init();
    req.payload = d_req;
    req.length = sizeof(dissemination_request);

    uint16_t code = codify(BROADCAST_FILE);
    dissemmination_request_add_to_header(d_req, &code, sizeof(uint16_t));

    int filename_size = strlen(meta->filename);
    uint32_t fn_size = htonl(filename_size);
    dissemmination_request_add_to_header(d_req, &fn_size, sizeof(uint32_t));
    dissemmination_request_add_to_header(d_req, meta->filename, filename_size);

    uint32_t mode_n = htonl(meta->mode);
    uint32_t version_n = htonl(meta->version);
    uint32_t creator_n = htonl(meta->creator);
    dissemmination_request_add_to_header(d_req, &mode_n, sizeof(uint32_t));
    dissemmination_request_add_to_header(d_req, &version_n, sizeof(uint32_t));
    dissemmination_request_add_to_header(d_req, &creator_n, sizeof(uint32_t));


    uint32_t file_size_n = htonl(meta->file_size);
    dissemmination_request_add_to_header(d_req, &file_size_n, sizeof(uint32_t));

    uint32_t i_n = htonl(i);
    uint32_t blocks_n = htonl(blocks);
    dissemmination_request_add_to_header(d_req, &i_n, sizeof(uint32_t)); //block number
    dissemmination_request_add_to_header(d_req, &blocks_n, sizeof(uint32_t));

    int block_size;
    if(i == blocks-1 && meta->file_size % state->block_size != 0)//last block
        block_size = meta->file_size % state->block_size; //in case of smaller than a block
    else
        block_size = state->block_size;

    uint32_t block_size_n = htonl(block_size);
    dissemmination_request_add_to_header(d_req, &block_size_n, sizeof(uint32_t));

    d_req->body = malloc(block_size);

    if (read_file_to_mem(d_req->body, block_size, meta, i, state))
        d_req->body_size = block_size;
    else {
        perror("Error on read file: ");
        free(d_req->body);
        free(d_req->header);
        YggRequest_freePayload(&req);
        return;
    }

    //printf("Requesting block %d out of %d\n", i, blocks);
    deliverRequest(&req);
    YggRequest_freePayload(&req);
}

static void request_dissemination(file_meta* meta, block_data_transfer_state* state) {

    int blocks = meta->file_size % state->block_size == 0 ? meta->file_size / state->block_size : (meta->file_size / state->block_size)+1;

    on_going_transfer* t = create_on_going_transfer(meta, strlen(meta->filename), blocks, state->on_going_transmission);

    int i = next_missing_block(t->blocks, t->n_blocks);
    request_block_dissemination(state, meta, i, blocks);
    mark_block(t->blocks, i);

}

static void report_error(YggRequest* req) {
    //TODO build error reply.
    printf("File is too large to transfer\n");
}

static void provide_msg_body(YggRequest* req, block_data_transfer_state* state) {
    dissemination_msg_request* p_req = req->payload;

    void* ptr = p_req->req->header + sizeof(uint16_t);

    file_meta meta;

    uint32_t filename_size_n;
    memcpy(&filename_size_n, ptr, sizeof(uint32_t)), ptr+= sizeof(uint32_t);
    int filename_size = ntohl(filename_size_n);
    meta.filename= malloc(filename_size+1);
    bzero(meta.filename, filename_size+1);
    memcpy(meta.filename, ptr, filename_size), ptr+= filename_size + sizeof(uint32_t); //skip mode


    uint32_t version, creator;
    memcpy(&version, ptr, sizeof(uint32_t)), ptr+= sizeof(uint32_t);
    memcpy(&creator, ptr, sizeof(uint32_t)), ptr+= sizeof(uint32_t);
    meta.version = ntohl(version);
    meta.creator = ntohl(creator);


    uint32_t file_size_n;
    memcpy(&file_size_n, ptr, sizeof(uint32_t)), ptr+= sizeof(uint32_t);
    meta.file_size = ntohl(file_size_n);


    uint32_t block_n;
    memcpy(&block_n, ptr, sizeof(uint32_t)), ptr+= sizeof(uint32_t);
    meta.block = ntohl(block_n);


    uint32_t n_blocks_n;
    memcpy(&n_blocks_n, ptr, sizeof(uint32_t)), ptr += sizeof(uint32_t);
    meta.n_blocks = ntohl(n_blocks_n);

    uint32_t block_size_n;
    memcpy(&block_size_n, ptr, sizeof(uint32_t));

    bool answer = false;
    if(is_same_version(&meta, state)) {
        answer = true;
    } else {
        on_going_transfer* t = list_find_item(state->on_going_reception, (equal_function) equal_file, meta.filename);
        if(t && t->version == meta.version && t->creator == meta.creator && is_marked(t->blocks, meta.block)) {
            answer = true;
        }
    }


    if(answer) {
        char *location = change_file_location(meta.filename, state);

        int block_size = ntohl(block_size_n);

        meta.fd = open_file(location);

        //printf("===================> Answering body request for block %d with size %d version %d:%d of file %s\n", meta.block , block_size, meta.version, meta.creator, location);

        p_req->req->body = malloc(block_size);
        if (read_file_to_mem(p_req->req->body, block_size, &meta, meta.block, state))
            p_req->req->body_size = block_size;
        else {
            perror("Error on read file: ");
            free(meta.filename);
            close(meta.fd);
            free(p_req->req->body);
            free(p_req->req->header);
            free(p_req->header);
            YggRequest_freePayload(req);
            return;
        }
        free(meta.filename);
        free(location);
        close(meta.fd);
    } else {
        p_req->req->body_size = 0;
        p_req->req->body = NULL;
    }
    req->proto_dest = req->proto_origin;
    req->proto_origin = state->my_id;
    req->request = REPLY;
    deliverReply(req);
}

static void process_request(YggRequest* req, block_data_transfer_state* state) {
    if(req->request_type == DISSEMINATE_FILE) {

        char* filename = (char*) req->payload;

        char* location = change_file_location(filename, state);
        int fd = open_file(location);
        file_meta* meta = process_file_meta(fd, filename, state);

        if(meta) {
            request_dissemination(meta, state);
            close(fd);
            free(meta);
        } else {
            close(fd);
            report_error(req);
        }
        free(location);
    } else if(req->proto_origin == state->dissemination_proto && req->request_type == MSG_BODY_REQ)
        provide_msg_body(req, state);

    YggRequest_freePayload(req);
}


static void process_file(YggMessage* msg, void* ptr, block_data_transfer_state* state) {


    file_meta* meta = malloc(sizeof(file_meta));

    uint32_t fn_size_n;
    ptr = YggMessage_readPayload(msg, ptr, &fn_size_n, sizeof(uint32_t));
    int filename_size = ntohl(fn_size_n);

    meta->filename = malloc(filename_size+1);
    bzero(meta->filename, filename_size+1);
    ptr = YggMessage_readPayload(msg, ptr, meta->filename, filename_size);

    uint32_t mode_n;
    uint32_t version_n;
    uint32_t creator_n;
    ptr = YggMessage_readPayload(msg, ptr, &mode_n, sizeof(uint32_t));
    ptr = YggMessage_readPayload(msg, ptr, &version_n, sizeof(uint32_t));
    ptr = YggMessage_readPayload(msg, ptr, &creator_n, sizeof(uint32_t));

    meta->mode = ntohl(mode_n);
    meta->version = ntohl(version_n);
    meta->creator = ntohl(creator_n);

    uint32_t file_size_n;
    ptr = YggMessage_readPayload(msg, ptr, &file_size_n, sizeof(uint32_t));
    meta->file_size = ntohl(file_size_n);

    uint32_t block_n;
    uint32_t n_blocks_n;
    ptr = YggMessage_readPayload(msg, ptr, &block_n, sizeof(uint32_t));
    ptr = YggMessage_readPayload(msg, ptr, &n_blocks_n, sizeof(uint32_t));
    meta->block = ntohl(block_n);
    meta->n_blocks = ntohl(n_blocks_n);

    uint32_t block_size_n;
    ptr = YggMessage_readPayload(msg, ptr, &block_size_n, sizeof(uint32_t));
    meta->block_size = ntohl(block_size_n);

    char* location = change_file_location(meta->filename, state);

    //printf("===================> Received block %d with size %d version %d:%d of file %s\n", meta->block , meta->block_size, meta->version, meta->creator, location);

    meta->fd = open_file(location);
    on_going_transfer* t = list_find_item(state->on_going_transmission, (equal_function) equal_file, meta->filename);
    if(t){
        int next = next_missing_block(t->blocks, t->n_blocks);
        if(next < 0) {
            //printf("Transmission of file %s is done\n", location);
            comple_transfer(meta, state->on_going_transmission, state);
        } else {
            request_block_dissemination(state, meta, next, meta->n_blocks);
            mark_block(t->blocks, next);
        }
    }
    close(meta->fd);


    bool new_version = is_new_version(meta, state);

    if(new_version) {
        t = list_find_item(state->on_going_reception, (equal_function) equal_file, meta->filename);
        if (!t) {
            t = create_on_going_transfer(meta, strlen(meta->filename), meta->n_blocks, state->on_going_reception);
        } else if(t->version < meta->version || (t->version == meta->version && t->creator < meta->creator)) {
            free(t->blocks);
            free(t);
            t = create_on_going_transfer(meta, strlen(meta->filename), meta->n_blocks, state->on_going_reception);
        } else {
            new_version = false;
        }


        if(!is_marked(t->blocks, meta->block)) {
            write_file_to_disk(ptr, meta, location, new_version);

            mark_block(t->blocks, meta->block);

            int next_missing = next_missing_block(t->blocks, t->n_blocks);
            time_t now = time(NULL);
            if (next_missing < 0) {
                char log_msg[200]; bzero(log_msg, 200);
                sprintf(log_msg, "Reception of file %s is done elapsed: %ds", location, now - t->start);
                ygg_log("BLOCK TRANSFER", "LOG", log_msg);
#if defined BLOCK_DEBUG
                fprintf(stderr, "Reception of file %s is done elapsed: %ds\n", location, now - t->start);
#endif
                comple_transfer(meta, state->on_going_reception, state);
            } else {
                char log_msg[200]; bzero(log_msg, 200);
                sprintf(log_msg, "Next missing block of file %s is %d got %d/%d elapsed: %ds", location, next_missing+1,
                        (t->n_blocks-total_missing_blocks(t->blocks, t->n_blocks)), t->n_blocks, now - t->start);
                ygg_log("BLOCK TRANSFER", "LOG", log_msg);
#if defined BLOCK_DEBUG
                fprintf(stderr, "Next missing block of file %s is %d  total missing: %d\n", location, next_missing,
                        total_missing_blocks(t->blocks, t->n_blocks));
#endif
            }
        }
    }

    free(meta->filename);
    free(location);
    free(meta);
}

static void process_msg(YggMessage* msg, block_data_transfer_state* state) {
    uint16_t code;
    void* ptr = YggMessage_readPayload(msg, NULL, &code, sizeof(uint16_t));
    block_data_transfer_msgs msg_type = decodify(code);

    switch(msg_type) {
        case BROADCAST_FILE:
            process_file(msg, ptr, state);
            break;
        case DIRECT_FILE:
            // process_copy(msg, ptr, state);
            break;
        case HANDSHAKE:
            // process_handshake(msg, ptr, state);
            break;
        case HANDSHAKE_REPLY:
            // process_handshake_reply(msg, ptr, state);
            break;

    }

    YggMessage_freePayload(msg); //if not destroyed, then should be destroyed

}


proto_def* block_data_transfer_mem_opt_init(block_data_transfer_args *args) {

    block_data_transfer_state* state = malloc(sizeof(block_data_transfer_state));
    state->my_id = PROTO_BLOCK_DATA_TRANSFER;
    state->dissemination_proto = args->dissemination_proto;
    state->dissemination_request = args->dissemination_request;
    state->dir = args->dir;
    state->block_size = sysconf(_SC_PAGE_SIZE);
    state->on_going_transmission = list_init();
    state->on_going_reception = list_init();

    char* dir_meta = change_file_location(".meta", state);
    state->dir_fd = open(dir_meta, O_CREAT | O_RDWR, 0666); //all read and write
//    state->staged_area = change_file_location(".staged", state);
//
//    struct stat st = {0};
//    if (stat(state->staged_area, &st) == -1) {
//        mkdir(state->staged_area, 0700);
//        perror(state->staged_area);
//    }

    IPAddr ip;
    getIpAddr(&ip);

    state->id = hash((char*)&ip, sizeof(IPAddr));

    proto_def* data_transfer = create_protocol_definition(PROTO_BLOCK_DATA_TRANSFER, "block data transfer", state, NULL);

    proto_def_add_msg_handler(data_transfer, (YggMessage_handler) process_msg);
    proto_def_add_request_handler(data_transfer, (YggRequest_handler) process_request);

    free(dir_meta);

    return data_transfer;
}