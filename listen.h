
#define LISTEN_EVENT_ADDED 1
#define LISTEN_EVENT_REMOVED 2
#define LISTEN_EVENT_RENAMED 3
#define LISTEN_EVENT_MODIFIED  4

struct listen_event {
    int type;
    char *dir;
    char *file;
};

int listen_forwarder_connect(char *host_name, int port);
struct listen_event* listen_forwarder_parse(char *json);
struct listen_event* listen_forwarder_read(int sockfd);
void listen_forwarder_print(struct listen_event *e);