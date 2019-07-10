//
// Created by Pedro Akos on 2019-07-08.
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

#include <getopt.h>


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

char * dir = NULL;


static void request_file_transfer(const char* filename) {

    int filename_size = strlen(filename);

    YggRequest req;
    YggRequest_init(&req, 400, PROTO_BLOCK_DATA_TRANSFER, REQUEST, DISSEMINATE_FILE);
    YggRequest_addPayload(&req, filename, filename_size+1);

    deliverRequest(&req);

}

char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

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

static bool read_file_transfer(int sock) {

    bool ret = true;

    uint16_t filename_size_n;
    readfully(sock, &filename_size_n, sizeof(uint16_t));
    int filename_size = ntohs(filename_size_n);
    char filename[filename_size]; bzero(filename, filename_size);
    readfully(sock, filename, filename_size);
    uint16_t mode_n;
    readfully(sock, &mode_n, sizeof(uint16_t));
    mode_t mode = ntohs(mode_n);

    uint16_t blocks_n;
    readfully(sock, &blocks_n, sizeof(uint16_t));
    int blocks = ntohs(blocks_n);

    char *pLastSlash = strrchr(filename, '/');
    char *pszBaseName = pLastSlash ? pLastSlash + 1 : filename;

    char* dir_path = concat(dir, "/");
    char* full_path = concat(dir_path, pszBaseName);
    free(dir_path);

    int fd = open(full_path, O_CREAT | O_RDWR | O_TRUNC, mode);

    if(fd > 0) {

        //TODO handle errors inside
        int block_size;
        for (int i = 0; i < blocks; i++) {
            uint16_t block_size_n;
            readfully(sock, &block_size_n, sizeof(uint16_t));
            int read_size = ntohs(block_size_n);
            if(i == 0)
                block_size = read_size;

            char buff[read_size];
            lseek(fd, block_size*i, SEEK_SET);
            readfully(sock, buff, read_size);
            writefully(fd, buff, read_size);
        }

        request_file_transfer(pszBaseName);

    } else {
        //TODO report some error;
        ret = false;
        perror("Error on open");
    }

    free(full_path);
    close(fd);
    return ret;

}

static void executeClientSession(int clientSocket, struct sockaddr_in* addr) {

    char clientIP[16];
    inet_ntop(AF_INET, &addr->sin_addr, clientIP, 16);
    printf("COMMAND SERVER - Starting session for a client located with address %s:%d\n", clientIP, addr->sin_port);

    uint16_t command;
    int stop = 0;
    char *reply = malloc(100);

    while (stop != 1) {
        bzero(reply, 100);
        if (readfully(clientSocket, &command, sizeof(uint16_t)) <= 0) { return; }
        int command_code = ntohs(command);
        switch (command_code) {
            case BUILD:
                request(BUILD);
                sprintf(reply, "Done");
                break;
            case SEND:
                request_with_payload(clientSocket, SEND);
                sprintf(reply, "Done");
                break;
            case FILE_TRANSFER:
                if(read_file_transfer(clientSocket)) {
                    sprintf(reply, "Commencing network dissemination");
                }else {
                    sprintf(reply, "An error occurred, check server's log for more details");
                }
                break;
            default:

                stop = 1;
                break;
        }


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
    address.sin_port = htons((unsigned short) 7189 /*server_port*/);

    if(bind(listen_socket, (const struct sockaddr*) &address, sizeof(address)) == 0 ) {

        if(listen(listen_socket, 20) < 0) {
            perror("COMMAND SERVER - Unable to setup listen on socket: ");
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
        //perror("CONTROL PROTOCOL SERVER - Unable to bind on listen socket: ");
        return;
    }

    return;

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


static void print_usage(const char* name) {
    printf("Usage: %s [-d working dir] <nodeAddress> <nodePort> [<contactAddress> <contactPort>]\n", name);
    printf("\t default working dir is ~/files/<nodePort>\n");
    printf("\t nodeAddress - IP address of the process\n");
    printf("\t nodePort - port of the process\n");
    printf("\t contactAddress - IP address of contact process\n");
    printf("\t contactPort - port of contact process\n");
    printf("\t if contact address and port omitted, the process is considered to be the first in the system\n");
}

int main(int argc, char* argv[]) {


    int index = 1;
    int opt;
    if((opt = getopt(argc, argv, "hd:")) != -1) {
        switch (opt) {
            case 'd':
                dir = optarg;
                if(optind >= argc) {
                    print_usage(argv[0]);
                    exit(1);
                } else
                    index = optind;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(1);
            default:
                print_usage(argv[0]);
                exit(1);
                break;
        }

    } else {
        if(argc < 2) {
            print_usage(argv[0]);
            exit(1);
        }
    }



    char* myIp = argv[index];
    index ++;
    unsigned short myport = atoi(argv[index]);

//    // only for local testing
    if(!dir) {
        struct stat st = {0};
        char* home = getenv("HOME");
        char* files_dir = concat(home, "/files");
        if(stat(files_dir, &st) == -1)
            mkdir(files_dir, 0700);

        char* tmp = concat(files_dir, "/");
        free(files_dir);
        files_dir = tmp;

        dir = concat(files_dir, argv[2]);
        if (stat(dir, &st) == -1) {
            mkdir(dir, 0700);
        }
        free(files_dir);
    }

    index++;
    char *contact;
    unsigned short contactport;
    if(index >= argc) {
        contact = myIp;
        contactport = myport;
    } else {
        contact = argv[index];
        index++;
        contactport = atoi(argv[index]);
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



    pthread_t t;
    pthread_create(&t, NULL, (gen_function) handle_client_requests, NULL);
    pthread_detach(t);



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
