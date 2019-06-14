//
// Created by João Leitão on 2019-06-09.
//

#include "api.h"


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


/*******************************************************
 *  IP sockets
 *******************************************************/

int setupIpChannel(Channel* ch, NetworkConfig* ntc) {
    //create the listen socket for the channel given the configuration
    IpNetworkConfig* ntconf = &ntc->config.ipntconf;

    ch->type = ntc->type;
    bzero(ch->ip.addr, 16);
    memcpy(ch->ip.addr, ntconf->ip.addr, 16); //TODO: ip address could be obtained from interface
    ch->ip.port = ntconf->ip.port;

    if(ntconf->sock_type == TCP) {
        ch->sockid = socket(PF_INET, SOCK_STREAM, 0);
        struct sockaddr_in address;
        bzero(&address, sizeof(struct sockaddr_in));
        address.sin_family = AF_INET;
        inet_pton(AF_INET, ch->ip.addr, &(address.sin_addr));
        address.sin_port = htons(ch->ip.port);
        if(bind(ch->sockid, (const struct sockaddr*) &address, sizeof(address)) == 0 ) {
            if(listen(ch->sockid, ntconf->max_pending_connections) < 0) {
                close(ch->sockid);
                perror("UNABLE TO LISTEN TO REQUESTED SOCKET");
                exit(2);
            }
        } else {
            close(ch->sockid);
            perror("UNABLE TO BIND REQUESTED SOCKET");
            exit(2);
        }

    }else { //TODO:
        printf("NOT SUPPORTED YET\n");
        exit(1);
    }

    return SUCCESS;
}

void set_sock_opt(int sockid, Channel* ch) {
    //iterate over channels socket options and apply them to socket

    //TODO: EMPTY FOR NOW
}
