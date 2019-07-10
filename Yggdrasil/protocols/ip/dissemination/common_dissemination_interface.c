//
// Created by Pedro Akos on 2019-07-01.
//

#include "common_dissemination_interface.h"

dissemination_msg_request* create_dissemination_msg_request(void* dissemination_proto_header) {
    dissemination_msg_request* req = malloc(sizeof(dissemination_msg_request));
    req->header = dissemination_proto_header;
    req->req = dissemination_request_init();

    return req;
}

dissemination_request* dissemination_request_init() {
    dissemination_request* req = malloc(sizeof(dissemination_request));
    req->header = NULL;
    req->body = NULL;
    req->body_size = 0;
    req->header_size = 0;

    return req;
}

void* dissemination_request_destroy(dissemination_request* req) {
    free(req);
}



void dissemmination_request_add_to_header(dissemination_request* req, void* item, int item_size) {
    if(req->header == NULL){
        req->header = malloc(item_size);
    }else{
        req->header = realloc(req->header, req->header_size + item_size);
    }

    memcpy(req->header+req->header_size, item, item_size);

    req->header_size += item_size;

}

void dissemmination_request_add_to_body(dissemination_request* req, void* item, int item_size) {
    if(req->body == NULL){
        req->body = malloc(item_size);
    }else{
        req->body = realloc(req->body, req->body_size + item_size);
    }

    memcpy(req->body+req->body_size, item, item_size);

    req->body_size += item_size;
}