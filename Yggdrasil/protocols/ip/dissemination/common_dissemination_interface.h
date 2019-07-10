//
// Created by Pedro Akos on 2019-07-01.
//

#ifndef YGGDRASIL_COMMON_DISSEMINATION_INTERFACE_H
#define YGGDRASIL_COMMON_DISSEMINATION_INTERFACE_H

#include <stdlib.h>
#include <string.h>

#define DISSEMINATION_REQUEST 77
#define MSG_BODY_REQ 99

typedef struct __dissemination_request {
    //void* ptrs are managed by this protocol

    unsigned short header_size;
    void* header;

    unsigned short body_size;
    void* body;
}dissemination_request;

typedef struct __dissemination_msg_request {

   void* header;
   dissemination_request* req;
}dissemination_msg_request;

dissemination_msg_request* create_dissemination_msg_request(void* dissemination_proto_header);

dissemination_request* dissemination_request_init();

void* dissemination_request_destroy(dissemination_request* req);

void dissemmination_request_add_to_header(dissemination_request* req, void* item, int item_size);
void dissemmination_request_add_to_body(dissemination_request* req, void* item, int item_size);

#endif //YGGDRASIL_COMMON_DISSEMINATION_INTERFACE_H
