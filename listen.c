#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "jsmn/jsmn.h"
#include "listen.h"
#include "debug.h"

#define MAX_EVENT_LENGTH 1024
#define EVENT_TOKENS 6

void print_token(char *json, jsmntok_t *t);
char* token_string(char *json, jsmntok_t *t);
int token_event_from_string(char *event);

int listen_forwarder_connect(char *address, int port) {
    int sockfd = 0;
    struct sockaddr_in serv_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0))< 0) {
        perror("listener socket()");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(address);

    DPRINTF("LISTENER: connecting to %s:%d\n", address, port);
    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0) {
        perror("listener connect()");
        return -1;
    }
    DPRINTF("LISTENER: connected on fd=%d\n", sockfd);

    return sockfd;
}

struct listen_event* listen_forwarder_parse(char *json) {
    jsmn_parser parser;
    jsmntok_t tokens[EVENT_TOKENS];

    jsmn_init(&parser);
    int r = jsmn_parse(&parser, json, strlen(json), tokens, EVENT_TOKENS);

    if (r != EVENT_TOKENS) {
        fprintf(stderr,"Expected %d tokens, got %d\n", EVENT_TOKENS, r);
        exit(1);
    }

    if (tokens[0].type != JSMN_ARRAY) {
        fprintf(stderr,"Expected an array at root\n");
        exit(1);
    }

    char *event_type = token_string(json, &tokens[2]);
    if (event_type == NULL) {
        fprintf(stderr,"Expected a string for event type\n");
        exit(1);
    }

    char *event_dir = token_string(json, &tokens[3]);
    if (event_dir == NULL) {
        fprintf(stderr,"Expected a string for event dir\n");
        exit(1);
    }

    char *event_file = token_string(json, &tokens[4]);
    if (event_file == NULL) {
        fprintf(stderr,"Expected a string for event file\n");
        exit(1);
    }

    struct listen_event *event = malloc(sizeof(struct listen_event));
    event->type = token_event_from_string(event_type);
    if (event->type == 0) {
        fprintf(stderr,"Unrecognized event type %s\n", event_type);
        exit(1);
    }

    event->dir = event_dir;
    event->file = event_file;

    return event;
}

struct listen_event* listen_forwarder_read(int sockfd) {
    uint32_t len;
    int n;

    DPRINTF("LISTENER: blocking on read(fd=%d)\n", sockfd);
    if ((n = read(sockfd, &len, sizeof(len))) == -1) {
        perror("listener read()");
        exit(1);
    }
    // DPRINTF("LISTENER: read returned %d\n", n);
    DPRINTF("LISTENER: read length header of 0x%x\n", len);

    len = ntohl(len);
    DPRINTF("LISTENER: read a length of %d from fd=%d\n", len, sockfd);

    if (len > MAX_EVENT_LENGTH) {
        fprintf(stderr,"event size exceeded max line length\n");
        exit(1);
    }

    uint32_t bytesReceived = 0;
    char buf[MAX_EVENT_LENGTH];
    memset(buf, 0, sizeof(buf));

    while(bytesReceived < len) {
        n = read(sockfd, &buf[bytesReceived], len-bytesReceived);
        if (n == -1) {
            perror("read()");
            exit(2);
        }
        if (n == 0) {
            fprintf(stderr,"remote closed connection mid event\n");
            exit(3);
        }
        bytesReceived += n;
    }

    char nl;
    if ((read(sockfd, &nl, 1) != 1) || nl != '\n') {
        fprintf(stderr,"expected a trailing newline on event\n");
        exit(3);
    }

    DPRINTF("LISTENER: Read event from forwarder\n");
    return listen_forwarder_parse(buf);
}

void listen_forwarder_print(struct listen_event *e) {
  printf(
    "listen_event{type:%d dir:%s file:%s}\n",
    e->type,
    e->dir,
    e->file
  );
}

void print_token(char *json, jsmntok_t *t) {
  int len = t->end - t->start;
  printf(
    "token{type:%d start:%d end:%d size:%d} ",
    t->type,
    t->start,
    t->end,
    t->size
  );
  switch (t->type) {
    case JSMN_ARRAY:
      printf("[array]");
      break;
    case JSMN_STRING:
      printf("%.*s", len, json + t->start);
      break;
    default:
      break;
  }
  printf("\n");
}

// TODO: free() str
char* token_string(char *json, jsmntok_t *t) {
    if (t->type != JSMN_STRING) {
        return NULL;
    }

    int len = t->end-t->start;
    char *str = (char*)malloc(len+1);
    memcpy(str, json+t->start, len);
    str[len] = '\0';

    return str;
}

int token_event_from_string(char *event) {
    switch(event[3]) {
        case 'e': return LISTEN_EVENT_ADDED;
        case 'o': return LISTEN_EVENT_REMOVED;
        case 'a': return LISTEN_EVENT_RENAMED;
        case 'i': return LISTEN_EVENT_MODIFIED;
    }
    return 0;
}