//
// Created by Pedro Akos on 2019-06-17.
//

#ifndef YGGDRASIL_PLUMTREE_MEM_OPTIMIZED_FLOW_CONTROL_H
#define YGGDRASIL_PLUMTREE_MEM_OPTIMIZED_FLOW_CONTROL_H

#include "plumtree_mem_optimized.h"
#include "../dispatcher/multi_tcp_socket_dispatcher.h"


typedef struct __plumtree_flow_control_args {
    plumtree_args* plumtreeArgs;
    int dispatcher_msg_size_threashold;
}plumtree_flow_control_args;


proto_def* plumtree_mem_optimized_flow_control_init(plumtree_flow_control_args* args);

plumtree_flow_control_args* plumtree_flow_control_args_init(int fanout, unsigned short timeout_s, long timeout_ns, unsigned short membership_id, int dispatcher_msg_size_threashold);
void plumtree_flow_control_args_destroy(plumtree_flow_control_args* args);

#endif //YGGDRASIL_PLUMTREE_MEM_OPTIMIZED_FLOW_CONTROL_H
