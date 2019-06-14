//
// Created by Pedro Akos on 2019-05-31.
//

#include "block_data_transfer.h"

typedef struct __block_data_transfer_state {

    short my_id;
    short dissemination_proto;
    short dissemination_request;

    off_t block_size;

    //local test dir
    char* dir;

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

    int block;
    int n_blocks;
    int block_size;
}file_meta;

static file_meta* process_file_meta(int fd, char* filename){
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

    return meta;
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

static bool write_file_to_disk(void* buffer, file_meta* meta) {

    meta->fd = open(meta->filename, O_CREAT | O_RDWR, meta->mode);

    off_t block_size = meta->block_size;
    if(meta->block > 0 && meta->block == meta->n_blocks-1 && meta->block_size != (meta->file_size/meta->n_blocks))
        block_size = (meta->file_size-meta->block_size)/(meta->n_blocks-1);

    printf("Seek file to off_set %ld\n", block_size*meta->block);
    lseek(meta->fd, block_size*meta->block, SEEK_SET);

    printf("Writing block %d out of %d of file %s to disk\n", meta->block, meta->n_blocks, meta->filename);
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
        printf("Written %ld out of %d\n", writen, meta->block_size);
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

static void request_dissemination(file_meta* meta, block_data_transfer_state* state) {


    int blocks = meta->file_size % state->block_size == 0 ? meta->file_size / state->block_size : (meta->file_size / state->block_size)+1;


    for(int i = 0; i < blocks; i++) {
        YggRequest req;
        YggRequest_init(&req, state->my_id, state->dissemination_proto, REQUEST, state->dissemination_request);
        uint16_t code = codify(BROADCAST_FILE);
        YggRequest_addPayload(&req, &code, sizeof(uint16_t));
        int filename_size = strlen(meta->filename);
        YggRequest_addPayload(&req, &filename_size, sizeof(int));
        YggRequest_addPayload(&req, meta->filename, filename_size);
        YggRequest_addPayload(&req, &meta->mode, sizeof(mode_t));
        YggRequest_addPayload(&req, &meta->file_size, sizeof(off_t));
        YggRequest_addPayload(&req, &i, sizeof(int));
        YggRequest_addPayload(&req, &blocks, sizeof(int));

        int block_size;
        if(i == blocks-1 && meta->file_size % state->block_size != 0)//last block
            block_size = meta->file_size % state->block_size; //in case of smaller than a block
        else
            block_size = state->block_size;

        YggRequest_addPayload(&req, &block_size, sizeof(int));

        req.payload = realloc(req.payload, req.length + block_size);
        if (read_file_to_mem(req.payload + req.length, block_size, meta, i, state))
            req.length += block_size;
        else {
            perror("Error on read file: ");
            YggRequest_freePayload(&req);
            return;
        }

        deliverRequest(&req);
        YggRequest_freePayload(&req);
    }
}

static void report_error(YggRequest* req) {
    //TODO build error reply.
    printf("File is too large to transfer\n");
}

static void process_request(YggRequest* req, block_data_transfer_state* state) {
    if(req->request_type == DISSEMINATE_FILE) {

        char* filename = (char*) req->payload;

        int fd = open_file(filename);
        file_meta* meta = process_file_meta(fd, filename);

        if(meta) {
            request_dissemination(meta, state);
            close(fd);
            free(meta);
        } else {
            close(fd);
            report_error(req);
        }
    }

    YggRequest_freePayload(req);
}

static char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

static void change_file_location(file_meta* meta, block_data_transfer_state* state) {

    int filename_size = strlen(meta->filename);
    char filename[filename_size+1];
    bzero(filename, filename_size+1);
    memcpy(filename, meta->filename, filename_size);

    char* str = strtok(filename,"/");;
    char file[200];
    while(str != NULL) {
        bzero(file, 200);
        memcpy(file, str, strlen(str));
        str = strtok(NULL,"/");
    }
    char* f = concat("/", file);
    free(meta->filename);
    meta->filename = concat(state->dir, f);

    free(f);
}


static void process_file(YggMessage* msg, void* ptr, block_data_transfer_state* state) {

    int filename_size;
    file_meta* meta = malloc(sizeof(file_meta));

    ptr = YggMessage_readPayload(msg, ptr, &filename_size, sizeof(int));

    meta->filename = malloc(filename_size+1);
    bzero(meta->filename, filename_size+1);
    ptr = YggMessage_readPayload(msg, ptr, meta->filename, filename_size);

    ptr = YggMessage_readPayload(msg, ptr, &meta->mode, sizeof(mode_t));
    ptr = YggMessage_readPayload(msg, ptr, &meta->file_size, sizeof(off_t));


    ptr = YggMessage_readPayload(msg, ptr, &meta->block, sizeof(int));
    ptr = YggMessage_readPayload(msg, ptr, &meta->n_blocks, sizeof(int));

    ptr = YggMessage_readPayload(msg, ptr, &meta->block_size, sizeof(int));


#if defined TEST_LOCAL_TRANSFER
    change_file_location(meta, state);
#endif

    write_file_to_disk(ptr, meta);
    free(meta->filename);
    free(meta);
}

static void process_msg(YggMessage* msg, block_data_transfer_state* state) {
    uint16_t code;
    void* ptr = YggMessage_readPayload(msg, NULL, &code, sizeof(uint16_t));
    block_data_transfer_msgs msg_type = decodify(code);

    switch(msg_type) {
        case BROADCAST_FILE:
            process_file(msg, ptr, state); //TODO broadcaster also receives!
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


proto_def* block_data_transfer_init(block_data_transfer_args* args) {

    block_data_transfer_state* state = malloc(sizeof(block_data_transfer_state));
    state->my_id = PROTO_BLOCK_DATA_TRANSFER;
    state->dissemination_proto = args->dissemination_proto;
    state->dissemination_request = args->dissemination_request;
    state->dir = args->dir;
    state->block_size = sysconf(_SC_PAGE_SIZE);

    proto_def* data_transfer = create_protocol_definition(PROTO_BLOCK_DATA_TRANSFER, "block data transfer", state, NULL);

    proto_def_add_msg_handler(data_transfer, (YggMessage_handler) process_msg);
    proto_def_add_request_handler(data_transfer, (YggRequest_handler) process_request);


    return data_transfer;
}


block_data_transfer_args* block_data_transfer_args_init(short dissemination_proto, short dissemination_request, char* dir){
    block_data_transfer_args* args = malloc(sizeof(block_data_transfer_args));
    args->dir = dir;
    args->dissemination_request = dissemination_request;
    args->dissemination_proto = dissemination_proto;

    return args;
}

void block_data_transfer_args_destroy(block_data_transfer_args* args) {
    free(args);
}