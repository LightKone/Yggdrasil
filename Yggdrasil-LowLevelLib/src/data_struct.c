//
// Created by João Leitão on 2019-06-09.
//

#include "data_struct.h"

/*********************************************************
 * NetworkConfig
 *********************************************************/

NetworkConfig* defineIpNetworkConfig(const char* ip_addr, unsigned short port, connection_type connection, int max_pending_connections, int keepalive) {
    NetworkConfig* ntconf = (NetworkConfig*) malloc(sizeof(struct __NetworkConfig));
    ntconf->type = IP;
    bzero(ntconf->config.ipntconf.ip.addr, 16);
    memcpy(ntconf->config.ipntconf.ip.addr, ip_addr, strlen(ip_addr));

    ntconf->config.ipntconf.ip.port = port;
    ntconf->config.ipntconf.sock_type = connection;
    ntconf->config.ipntconf.keepalive = 1; //TODO: Use this for something
    ntconf->config.ipntconf.max_pending_connections = max_pending_connections;

    return ntconf;
}

