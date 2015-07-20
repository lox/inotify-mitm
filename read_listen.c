#include <stdio.h>
#include <stdlib.h>
#include "listen.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr,"usage: read_listen hostname port\n");
        return 1;
    }

    int sock = listen_forwarder_connect(argv[1], atoi(argv[2]));
    if (sock <= 0) {
        perror("listen_forwarder_connect()");
        exit(1);
    }

    struct listen_event *e;

    while((e = listen_forwarder_read(sock)) != NULL) {
        listen_forwarder_print(e);
    }
}