//
// Created by Pedro Akos on 2019-06-06.
//

#ifndef YGGDRASIL_BLOCK_DATA_TRANSFER_H
#define YGGDRASIL_BLOCK_DATA_TRANSFER_H


#include <sys/mman.h>
#include <sys/types.h>

#include <utime.h> //TODO add this to this protocol

#include <sys/stat.h>
#include <fcntl.h>

#include <limits.h>
#include <errno.h>


#include "core/ygg_runtime.h"


#define PROTO_BLOCK_DATA_TRANSFER 367

#define DISSEMINATE_FILE 44

typedef struct _block_data_transfer_args{
    short dissemination_proto;
    short dissemination_request;

    char* dir;

}block_data_transfer_args;


proto_def* block_data_transfer_init(block_data_transfer_args* args);

block_data_transfer_args* block_data_transfer_args_init(short dissemination_proto, short dissemination_request, char* dir);
void block_data_transfer_args_destroy(block_data_transfer_args* args);


#endif //YGGDRASIL_BLOCK_DATA_TRANSFER_H
