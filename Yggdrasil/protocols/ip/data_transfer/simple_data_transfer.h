//
// Created by Pedro Akos on 2019-05-31.
//

#ifndef YGGDRASIL_SIMPLE_DATA_TRANSFER_H
#define YGGDRASIL_SIMPLE_DATA_TRANSFER_H


#include <sys/mman.h>
#include <sys/types.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <limits.h>

#include <errno.h>

#include "core/ygg_runtime.h"



#define PROTO_SIMPLE_DATA_TRANSFER 366

#define DISSEMINATE_FILE 44

typedef struct _simple_data_transfer_args{
    short dissemination_proto;
    short dissemination_request;

    char* dir;

}simple_data_transfer_args;


proto_def* simple_data_transfer_init(simple_data_transfer_args* args);

simple_data_transfer_args* simple_data_transfer_args_init(short dissemination_proto, short dissemination_request, char* dir);
void simple_data_transfer_args_destroy(simple_data_transfer_args* args);


#endif //YGGDRASIL_SIMPLE_DATA_TRANSFER_H
