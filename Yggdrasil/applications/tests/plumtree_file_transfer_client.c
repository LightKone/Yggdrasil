/*********************************************************
 * This code was written in the context of the Lightkone
 * European project.
 * Code is of the authorship of NOVA (NOVA LINCS @ DI FCT
 * NOVA University of Lisbon)
 * Authors:
 * Pedro Ákos Costa (pah.costa@campus.fct.unl.pt)
 * João Leitão (jc.leitao@fct.unl.pt)
 * (C) 2018
 *********************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <arpa/inet.h>


#define BUFSIZE 50

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

char* read_line(void)
{
    int bufsize = BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;

    if (!buffer) {
        fprintf(stderr, "allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Read a character
        c = getchar();

        // If we hit EOF, replace it with a null character and return.
        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            return buffer;
        } else {
            buffer[position] = c;
        }
        position++;

        // If we have exceeded the buffer, reallocate.
        if (position >= bufsize) {
            bufsize += BUFSIZE;
            buffer = realloc(buffer, bufsize);
            if (!buffer) {
                fprintf(stderr, "allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

int executeCommandSetup(int sock) {
    int command = BUILD;
    return writefully(sock, &command, sizeof(int));
}



void executeCommandMessage(int sock) {
    int command = SEND;
    printf("prompt messages:\n");
    char* exec = read_line();

    writefully(sock, &command, sizeof(int));
    int exec_size = strlen(exec) + 1;
    writefully(sock, &exec_size, sizeof(int));
    writefully(sock, exec, exec_size);

}


void executeCommandTransfer(int sock) {
    int command = FILE_TRANSFER;
    printf("prompt file path:\n");
    char* exec = read_line();

    writefully(sock, &command, sizeof(int));
    int exec_size = strlen(exec) + 1;
    writefully(sock, &exec_size, sizeof(int));
    //TODO verify if file exists
    writefully(sock, exec, exec_size);

}



int prompt(int sock) {
    printf("Available operations:\n");
    printf("0: initialize control topology\n");
    printf("1: send message\n");
    printf("2: trasnfer file\n");
    printf("other: exit\n");

    char* command = read_line();

    int code = atoi(command);
    free(command);

    char* reply = NULL;

    switch(code) {
        case BUILD:
            executeCommandSetup(sock);
            reply = getResponse(sock);
            break;
        case SEND:
            executeCommandMessage(sock);
            reply = getResponse(sock);

            break;
        case FILE_TRANSFER:
            executeCommandTransfer(sock);
            reply = getResponse(sock);
            break;
        default:
            return 1;
        break;
    }

    if(reply != NULL) {
        printf("Response: %s\n", reply);
        free(reply);
        return 0;
    } else {
        printf("An error has occurred\n");
        return 1;
    }

    return 0;
}

int main(int argc, char* argv[]) {

    if(argc != 3) {
        printf("Usage: %s IP PORT\n", argv[0]);
        return 1;
    }

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    bzero(&address, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    inet_aton(argv[1], &address.sin_addr);
    address.sin_port = htons( atoi(argv[2]) );
    int success = connect(sock, (const struct sockaddr*) &address, sizeof(address));

    if(success == 0) {

        while(prompt(sock) == 0);

        close(sock);
    } else {
        printf("Unable to connect to server.\n");
        return 1;
    }

    return 0;
}
