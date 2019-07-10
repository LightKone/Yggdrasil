//
// Created by Pedro Akos on 2019-07-01.
//

#ifndef YGGDRASIL_FLOOD_H
#define YGGDRASIL_FLOOD_H

#include "ygg_runtime.h"
#include "utils/hashfunctions.h"

#include "protocols/ip/membership/hyparview.h"

#include "common_dissemination_interface.h"


#define PROTO_FLOOD 214

typedef struct __flood_args {

}flood_args;

proto_def* flood_init(flood_args* args);

flood_args* flood_args_init();
void flood_args_destroy(flood_args* args);

#endif //YGGDRASIL_FLOOD_H
