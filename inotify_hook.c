#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "listen.h"
#include "debug.h"

#ifndef RTLD_NEXT
#define RTLD_NEXT ((void *) -1l)
#endif

#define REAL_LIBC RTLD_NEXT
#define MAX_WATCHES 10000
#define MAX_PATH_LENGTH 10000

int inotify_pipe[2];
int inotify_fd;
int listener_sock;

struct watch_entry {
    int wd;
    dev_t device;
    ino_t inode;
};

struct watch_entry watches[MAX_WATCHES];
struct watch_entry *watches_head = watches;

int wd_for_path(char *pathname) {
    struct stat st;
    if (stat(pathname, &st) == -1) {
        return -1;
    }
    DPRINTF("SEARCH: looking for dev:%ld inode:%ld\n", st.st_dev, st.st_ino);

    for(struct watch_entry *w = watches; w != watches_head; w++) {
        if (w->device == st.st_dev && w->inode == st.st_ino) {
            return w->wd;
        }
    }

    errno = ENOENT;
    return -1;
}

void print_inotify_event(struct inotify_event *i) {
    DPRINTF("inotify_event: wd:%2d ", i->wd);

    if (i->cookie > 0) {
        DPRINTF("cookie:%4d; ", i->cookie);
    }

    DPRINTF("mask:");
    if (i->mask & IN_ACCESS)        DPRINTF("IN_ACCESS ");
    if (i->mask & IN_ATTRIB)        DPRINTF("IN_ATTRIB ");
    if (i->mask & IN_CLOSE_NOWRITE) DPRINTF("IN_CLOSE_NOWRITE ");
    if (i->mask & IN_CLOSE_WRITE)   DPRINTF("IN_CLOSE_WRITE ");
    if (i->mask & IN_CREATE)        DPRINTF("IN_CREATE ");
    if (i->mask & IN_DELETE)        DPRINTF("IN_DELETE ");
    if (i->mask & IN_DELETE_SELF)   DPRINTF("IN_DELETE_SELF ");
    if (i->mask & IN_IGNORED)       DPRINTF("IN_IGNORED ");
    if (i->mask & IN_ISDIR)         DPRINTF("IN_ISDIR ");
    if (i->mask & IN_MODIFY)        DPRINTF("IN_MODIFY ");
    if (i->mask & IN_MOVE_SELF)     DPRINTF("IN_MOVE_SELF ");
    if (i->mask & IN_MOVED_FROM)    DPRINTF("IN_MOVED_FROM ");
    if (i->mask & IN_MOVED_TO)      DPRINTF("IN_MOVED_TO ");
    if (i->mask & IN_OPEN)          DPRINTF("IN_OPEN ");
    if (i->mask & IN_Q_OVERFLOW)    DPRINTF("IN_Q_OVERFLOW ");
    if (i->mask & IN_UNMOUNT)       DPRINTF("IN_UNMOUNT ");

    if (i->len > 0) {
        DPRINTF("name:%s(%d)\n", i->name, i->len);
    } else {
        DPRINTF("\n");
    }
}

void inject_inotify_event(int pipefd, int sock) {
    struct listen_event *le = listen_forwarder_read(sock);

    DPRINTF("HOOK: read listen_event from socket:");
    #ifdef DEBUG
    listen_forwarder_print(le);
    #endif

    size_t event_size = sizeof(struct inotify_event) + strlen(le->file) + 1;
    struct inotify_event *event = malloc(event_size);

    int wd = wd_for_path(le->dir);
    if (wd == -1) {
        perror("wd_for_path()");
        exit(1);
    }

    event->wd = wd;
    event->cookie = 0;
    event->len = strlen(le->file)+1;
    event->mask = 0;

    if(le->type == LISTEN_EVENT_ADDED) event->mask |= IN_CREATE;
    if(le->type == LISTEN_EVENT_REMOVED) event->mask |= IN_DELETE;
    if(le->type == LISTEN_EVENT_RENAMED) event->mask |= IN_MOVE_SELF;
    if(le->type == LISTEN_EVENT_MODIFIED) event->mask |= IN_MODIFY;

    strcpy((char*)&event->name, le->file);

    DPRINTF("HOOK: injecting synthetic ");
    print_inotify_event(event);

    ssize_t n = write(pipefd, event, event_size);
    if (n == -1) {
        perror("write()");
    } else {
        DPRINTF ("HOOK: wrote %zd bytes of inotify_event to fd=%d\n", n, pipefd);
    }
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout) {
    static int (*func_select) (int, fd_set*, fd_set*, fd_set*, struct timeval*) = NULL;

    if (! func_select) {
        func_select = (int (*) (int, fd_set*, fd_set*, fd_set*, struct timeval*)) dlsym (REAL_LIBC, "select");
    }

    if (!inotify_pipe[0] || ! FD_ISSET(inotify_pipe[0], readfds)) {
        return func_select(nfds, readfds, writefds, errorfds, timeout);
    }

    DPRINTF("HOOK: blocking on select() on inotify fd=%d\n", inotify_pipe[0]);

    // inject the network listener socket into the read fds
    FD_SET(listener_sock, readfds);
    if ((int)listener_sock >= nfds) {
        nfds = (int) listener_sock+1;
    }

    int count = func_select(nfds, readfds, writefds, errorfds, timeout);
    DPRINTF("HOOK: done blocking on select(), got %d\n", count);

    if (FD_ISSET(listener_sock, readfds)) {
        inject_inotify_event(inotify_pipe[1], listener_sock);
        FD_SET(inotify_pipe[0], readfds);
    }

    return count;
}

ssize_t read (int fd, void *buf, size_t count) {

    static ssize_t (*func_read) (int, const void*, size_t) = NULL;

    if (! func_read) {
        func_read = (ssize_t (*) (int, const void*, size_t)) dlsym (REAL_LIBC, "read");
    }

    if (inotify_pipe[0] && fd == inotify_pipe[0]) {
        DPRINTF ("HOOK: inotify read(%d, buf, %zd)\n", fd, count);
    }

    return func_read (fd, buf, count);
}

int inotify_init (void) {
    static int (*func_init) (void) = NULL;

    if (! func_init) {
        func_init = (int (*) (void)) dlsym (REAL_LIBC, "inotify_init");
    }

    inotify_fd = func_init();
    DPRINTF ("HOOK: real inotify_init() got fd=%d\n", inotify_fd);

    if (pipe(inotify_pipe) != 0) {
        perror("pipe()");
        exit(1);
    }

    DPRINTF ("HOOK: replacing inotify fd with pipe=%d=>%d\n", inotify_pipe[0],inotify_pipe[1]);
    return inotify_pipe[0];
}

int inotify_rm_watch (int fd, int wd) {
    static int (*func_rm_watch) (int, int) = NULL;

    if (! func_rm_watch) {
        func_rm_watch = (int (*) (int, int)) dlsym (REAL_LIBC, "inotify_rm_watch");
    }

    DPRINTF ("HOOK: inotify_rm_watch on fd=%d, wd=%d\n", fd, wd);
    return func_rm_watch (fd, wd);
}

int inotify_add_watch (int fd, const char *pathname, uint32_t mask) {
    static int (*func_add_watch) (int, const char*, uint32_t) = NULL;

    if (! func_add_watch) {
        func_add_watch = (int (*) (int, const char*, uint32_t)) dlsym (REAL_LIBC, "inotify_add_watch");
    }

    if (! listener_sock && getenv("LISTEN_HOST") != NULL) {
        int sock = listen_forwarder_connect(getenv("LISTEN_HOST"), 9400);
        if (sock <= 0) {
            perror("listen_forwarder_connect()");
            exit(1);
        }
        listener_sock = sock;
    }

    if (fd == inotify_pipe[0]) {
        fd = inotify_fd;
    }

    int wd;
    wd = func_add_watch (fd, pathname, mask);

    struct stat st;
    if (stat(pathname, &st) == -1) {
        perror("stat()");
        exit(1);
    }

    DPRINTF("stat returned dev=%ld inode=%ld\n", (long)st.st_dev, (long)st.st_ino);
    watches_head->wd = wd;
    watches_head->device = st.st_dev;
    watches_head->inode = st.st_ino;
    watches_head++;

    DPRINTF ("HOOK: inotify_add_watch(%d, %s, 0x%x) returned (wd=%d)\n", inotify_fd, pathname, mask, wd);
    return wd;
}
