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

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <getopt.h>


typedef enum _commands {
    BUILD,
    SEND,
    FILE_TRANSFER
}commands;

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


char* getResponse(int sock) {
    char* answer = NULL;
    int size;
    int r = readfully(sock, &size, sizeof(int));
    if(! (r <= 0)) {
        answer = malloc(size);
        r = readfully(sock, answer, size);
        if(r <= 0) {
            free(answer);
            answer = NULL;
        }
    }
    return answer;
}

static void print_progress(const char* file, int  i, int blocks, off_t left) {
    printf("Progress: file %s %d/%d left: %ld\n", file, i+1,blocks, left);
}

int executeCommandTransfer(int sock, const char* file) {


    int fd = open(file, O_RDONLY);

    if(fd > 0) {

        struct stat s;
        if(fstat(fd, &s) < 0){
            close(fd);
            return -1;
        }

        int command = FILE_TRANSFER;

        uint16_t command_n = htons(command);
        writefully(sock, &command_n, sizeof(uint16_t));
        int filename_size = strlen(file) + 1;
        uint16_t filename_size_n = htons(filename_size);
        writefully(sock, &filename_size_n, sizeof(uint16_t));
        //TODO verify if file exists
        writefully(sock, file, filename_size);

        uint16_t mode_n = htons(s.st_mode);
        writefully(sock, &mode_n, sizeof(uint16_t));


        int block_size = sysconf(_SC_PAGE_SIZE);

        int blocks = s.st_size % block_size == 0 ? s.st_size / block_size : (s.st_size / block_size)+1;

        uint16_t blocks_n = htons(blocks);
        writefully(sock, &blocks_n, sizeof(uint16_t));

        off_t left = s.st_size;
        int i = 0;
        while(left > 0) {

            int read_size = (i == blocks-1 && s.st_size % block_size != 0) ? s.st_size % block_size : block_size;

            void *ptr;
            if ((ptr = mmap(NULL, read_size, PROT_READ, MAP_PRIVATE, fd, block_size * i)) != MAP_FAILED) {

                uint16_t  read_size_n = htons(read_size);

                writefully(sock, &read_size_n, sizeof(uint16_t));
                writefully(sock, ptr, read_size);

                left -= read_size;
                print_progress(file, i, blocks, left);
                i ++;
                munmap(ptr, block_size);
            } else {
                munmap(ptr, block_size);
                close(fd);
                return -1;
            }
        }

    }
    close(fd);
    return -1;

}


static void print_usage(const char* name) {
    printf("Usage: %s [-s server address] path/to/file\n", name);
    printf("\t default server address is 127.0.0.1\n");
}

int main(int argc, char* argv[]) {

    char server_addr[16];
    bzero(server_addr, 16);

    unsigned short server_port = 7189;

    int index = 1;
    int opt;
    if((opt = getopt(argc, argv, "s:")) != -1) {
        switch (opt) {
            case 's':
                strcpy(server_addr, optarg);
                if(optind >= argc) {
                    print_usage(argv[0]);
                    exit(1);
                } else
                    index = optind;
                break;
            default:
                print_usage(argv[0]);
                exit(1);
                break;
        }

    } else {
        if(argc != 2) {
            print_usage(argv[0]);
            exit(1);
        }
    }

    if(!server_addr[0]) {
        strcpy(server_addr, "127.0.0.1");
    }

    char* file = argv[index];

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    bzero(&address, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    inet_aton(server_addr, &address.sin_addr);
    address.sin_port = htons( server_port );
    int success = connect(sock, (const struct sockaddr*) &address, sizeof(address));

    char* reply = NULL;
    if(success == 0) {
        if(executeCommandTransfer(sock, file)) {
            reply = getResponse(sock);
            printf("Response: %s\n", reply);
            free(reply);
        } else
            perror("An error occurred: ");
        close(sock);
    } else {
        printf("Unable to connect to server.\n");
        return 1;
    }

    return 0;
}
