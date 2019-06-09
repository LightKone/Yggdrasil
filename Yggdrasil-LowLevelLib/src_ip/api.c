//
// Created by João Leitão on 2019-06-09.
//

#include "api.h"

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
