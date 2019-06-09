//
// Created by João Leitão on 2019-06-09.
//

#ifndef YGGDRASIL_API_H
#define YGGDRASIL_API_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "../src/data_struct.h"

/*******************************************************
 *  IP sockets
 *******************************************************/

int setupIpChannel(Channel* ch, NetworkConfig* ntc);

void set_sock_opt(int sockid, Channel* ch);

#endif //YGGDRASIL_API_H
