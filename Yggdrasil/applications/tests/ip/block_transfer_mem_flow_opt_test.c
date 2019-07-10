//
// Created by Pedro Akos on 2019-06-07.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <sys/mman.h>

#include <sys/stat.h>
#include <fcntl.h>

#include "core/ygg_runtime.h"

#include "protocols/ip/dispatcher/multi_tcp_socket_dispatcher.h"
#include "protocols/ip/membership/hyparview.h"
#include "protocols/ip/dissemination/plumtree_mem_optimized_flow_control.h"

#include "protocols/ip/data_transfer/block_data_transfer_mem_opt.h"

typedef enum _commands {
    BUILD,
    SEND,
    FILE_TRANSFER
}commands;

char * dir;

static int readfully(int fd, void* buf, int len) {
    int missing = len;
    while(missing > 0) {
        int r = read(fd, buf + len - missing, missing);
        if(r <= 0)
            return r;
        missing-=r;
    }
    return len-missing;
}

static int writefully(int fd, void* buf, int len) {
    int missing = len;
    while(missing > 0) {
        int w = write(fd, buf + len - missing, missing);
        if(w <= 0)
            return w;
        missing-=w;
    }
    return len-missing;
}

static void request( commands cmd) {
    YggRequest req;
    YggRequest_init(&req, 400, 400, REQUEST, cmd);
    YggRequest_addPayload(&req, &cmd, sizeof(commands));
    deliverRequest(&req);
    YggRequest_freePayload(&req);
}

static void request_with_payload(int clientSocket, commands cmd) {
    YggRequest req;
    YggRequest_init(&req, 400, 400, REQUEST, cmd );
    readfully(clientSocket, &req.length, sizeof(int));
    req.payload = malloc(req.length+ sizeof(commands));
    memcpy(req.payload, &cmd, sizeof(commands));
    readfully(clientSocket, req.payload+sizeof(commands), req.length);
    req.length += sizeof(commands);
    deliverRequest(&req);
    YggRequest_freePayload(&req);
}


static void executeClientSession(int clientSocket, struct sockaddr_in* addr) {

    char clientIP[16];
    inet_ntop(AF_INET, &addr->sin_addr, clientIP, 16);
    printf("COMMAND SERVER - Starting session for a client located with address %s:%d\n", clientIP, addr->sin_port);

    int command_code;
    int stop = 0;
    char *reply = malloc(50);

    while (stop != 1) {
        bzero(reply, 50);
        if (readfully(clientSocket, &command_code, sizeof(int)) <= 0) { return; }

        switch (command_code) {
            case BUILD:
                request(BUILD);
                break;
            case SEND:
                request_with_payload(clientSocket, SEND);
                break;
            case FILE_TRANSFER:
                request_with_payload(clientSocket, FILE_TRANSFER);
                break;
            default:

                stop = 1;
                break;
        }

        sprintf(reply, "Done");
        int lenght = strlen(reply) + 1;

        writefully(clientSocket, &lenght, sizeof(int));
        writefully(clientSocket, reply, lenght);
    }
    printf("COMMAND SERVER - Terminating current session with client\n");
    close(clientSocket);

}


void handle_client_requests() {


    int listen_socket = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    bzero(&address, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((unsigned short) 10000 /*server_port*/);

    if(bind(listen_socket, (const struct sockaddr*) &address, sizeof(address)) == 0 ) {

        if(listen(listen_socket, 20) < 0) {
            perror("CONTROL PROTOCOL SERVER - Unable to setup listen on socket: ");
            return;
        }

        while(1) {
            //Main control cycle... we handle a single client each time...
            bzero(&address, sizeof(struct sockaddr_in));
            unsigned int length = sizeof(struct sockaddr_in);
            int client_socket = accept(listen_socket, (struct sockaddr*) &address, &length);

            executeClientSession(client_socket, &address);
        }

    } else {
        perror("CONTROL PROTOCOL SERVER - Unable to bind on listen socket: ");
        return;
    }

    return;

}

static void request_file_transfer(YggRequest* req) {
    commands cmd;
    void* ptr = YggRequest_readPayload(req,  NULL, &cmd, sizeof(commands));
    int file_name_size = strlen((char*)ptr);
    char* filename = malloc(file_name_size+1);
    bzero(filename, file_name_size+1);
    memcpy(filename, ptr, file_name_size);

    YggRequest_freePayload(req);
    YggRequest_init(req, 400, PROTO_BLOCK_DATA_TRANSFER, REQUEST, DISSEMINATE_FILE);
    YggRequest_addPayload(req, filename, file_name_size+1);

    deliverRequest(req);
    free(filename);
}


static void build_plumtree_request(YggRequest* req) {
    void* payload = req->payload;

    dissemination_request* d_req = dissemination_request_init();
    req->payload = d_req;

    dissemmination_request_add_to_header(d_req, payload, sizeof(commands));
    if(req->length > sizeof(commands))
        dissemmination_request_add_to_body(d_req, payload+ sizeof(commands), req->length- sizeof(commands));

    free(payload);
    req->length = sizeof(dissemination_request);
}

char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}



int main(int argc, char* argv[]) {

    char* myIp = argv[1];
    unsigned short myport = atoi(argv[2]);

//    // only for local testing
    struct stat st = {0};
//    dir = "~/files";
//    if(stat(dir, &st) == -1)
//        mkdir(dir, 0700);
//
//
//    dir = concat("~/files/", argv[2]);
//    if (stat(dir, &st) == -1) {
//        mkdir(dir, 0700);
//    }

    char* contact = argv[3];
    unsigned short contactport = atoi(argv[4]);

    int serve_command = atoi(argv[5]);

    dir = argv[6];
    if (stat(dir, &st) == -1) {
        mkdir(dir, 0700);
        perror(dir);
   }

    NetworkConfig* ntconf = defineIpNetworkConfig(myIp, myport, TCP, 10, 0);

    ygg_runtime_init(ntconf);

    multi_tcp_dispatch_args mtda;
    mtda.msg_size_threashold = 1024;
    overrideDispatcherProtocol(multi_tcp_socket_dispatcher_init, &mtda);


    hyparview_args* a = hyparview_args_init(contact, contactport, 4, 7, 4, 2, 2, 3, 5, 0, 1, 0);
    registerProtocol(PROTO_HYPARVIEW, hyparview_init, a);
    hyparview_args_destroy(a);

    plumtree_flow_control_args* p = plumtree_flow_control_args_init(0, 5, 0, PROTO_HYPARVIEW, 1024);
    registerProtocol(PROTO_PLUMTREE, (Proto_init) plumtree_mem_optimized_flow_control_init, p);
    plumtree_flow_control_args_destroy(p);

    block_data_transfer_args* dt = block_data_transfer_args_init(PROTO_PLUMTREE, PLUMTREE_BROADCAST_REQUEST, dir);
    registerProtocol(PROTO_BLOCK_DATA_TRANSFER, (Proto_init) block_data_transfer_mem_opt_init, dt);
    block_data_transfer_args_destroy(dt);


    short myId = 400;

    app_def* myapp = create_application_definition(myId, "MyApp");

    app_def_add_consumed_events(myapp, PROTO_HYPARVIEW, OVERLAY_NEIGHBOUR_UP);
    app_def_add_consumed_events(myapp, PROTO_HYPARVIEW, OVERLAY_NEIGHBOUR_DOWN);

    queue_t* inBox = registerApp(myapp);

    ygg_runtime_start();


    if(serve_command) {
        pthread_t t;
        pthread_create(&t, NULL, (gen_function) handle_client_requests, NULL);
    }


    while(true) {
        queue_t_elem elem;
        queue_pop(inBox, &elem);

        if(elem.type == YGG_REQUEST) {
            YggRequest* req = &elem.data.request;
            if(req->proto_origin == 400 && req->request == REQUEST) {
                switch (req->request_type) {
                    case BUILD:
                    case SEND:
                        req->request_type = PLUMTREE_BROADCAST_REQUEST;
                        req->proto_dest = PROTO_PLUMTREE;
                        build_plumtree_request(req);
                        deliverRequest(req);
                        break;
                    case FILE_TRANSFER:
                        request_file_transfer(req);
                        break;
                }
            }
            YggRequest_freePayload(req);
        }

        if(elem.type == YGG_EVENT) {
            IPAddr ip;
            void* ptr = YggEvent_readPayload(&elem.data.event, NULL, ip.addr, 16);
            YggEvent_readPayload(&elem.data.event, ptr, &ip.port, sizeof(unsigned short));
            char s[50];
            bzero(s, 50);
            sprintf(s, "%s %d", ip.addr, ip.port);

            if(elem.data.event.notification_id == OVERLAY_NEIGHBOUR_UP) {
                ygg_log("MYAPP", "NEIGHBOUR UP", s);
            } else if (elem.data.event.notification_id == OVERLAY_NEIGHBOUR_DOWN) {
                ygg_log("MYAPP", "NEIGHBOUR DOWN", s);
            }
        }

        if(elem.type == YGG_MESSAGE) {

            int command;

            void* ptr = YggMessage_readPayload(&elem.data.msg, NULL, &command, sizeof(int));

            switch (command) {
                case BUILD:
                    //ignore
                    ygg_log("APP", "BUILD", "received build command");
                    break;
                case SEND:
                    ygg_log("APP", "RECEIVE", (char*) ptr);
                    break;
            }

            YggMessage_freePayload(&elem.data.msg);
        }

        //do nothing for now
    }


}