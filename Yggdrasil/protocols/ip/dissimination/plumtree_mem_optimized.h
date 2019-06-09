//
// Created by Pedro Akos on 2019-06-07.
//

#ifndef YGGDRASIL_PLUMTREE_MEM_OPTIMIZED_H
#define YGGDRASIL_PLUMTREE_MEM_OPTIMIZED_H


#include "plumtree.h"



#define PLUMTREE_REQUEST_MSG_BODY 78


typedef struct __dissemination_request {
    //void* ptrs are managed by this protocol

    unsigned short header_size;
    void* header;

    unsigned short body_size;
    void* body;
}dissemination_request;

typedef struct __plumtree_request_msg_body_header plumtree_request_msg_body_header;

typedef struct __plumtree_request_msg_body {

    plumtree_request_msg_body_header* header;
    dissemination_request* req;
}plumtree_request_msg_body;

dissemination_request* dissemination_request_init();

void* dissemination_request_destroy(dissemination_request* req);

void dissemmination_request_add_to_header(dissemination_request* req, void* item, int item_size);
void dissemmination_request_add_to_body(dissemination_request* req, void* item, int item_size);


proto_def* plumtree_mem_optimized_init(plumtree_args* args);

#endif //YGGDRASIL_PLUMTREE_MEM_OPTIMIZED_H
