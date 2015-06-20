#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <sys/inotify.h>
#include <dlfcn.h>

#define DPRINTF(format, args...)	fprintf(stderr, format, ## args)

#ifndef RTLD_NEXT
#define RTLD_NEXT ((void *) -1l)
#endif

#define REAL_LIBC RTLD_NEXT

ssize_t read (int fd, void *buf, size_t count) {
	static ssize_t (*func_read) (int, const void*, size_t) = NULL;

	if (! func_read)
		func_read = (ssize_t (*) (int, const void*, size_t)) dlsym (REAL_LIBC, "read");

	DPRINTF ("HOOK: read %zd bytes from file descriptor (fd=%d)\n", count, fd);
	return func_read (fd, buf, count);
}

int inotify_add_watch (int fd, const char *pathname, uint32_t mask) {
	static int (*func_add_watch) (int, const char*, uint32_t) = NULL;

	if (! func_add_watch)
		func_add_watch = (int (*) (int, const char*, uint32_t)) dlsym (REAL_LIBC, "inotify_add_watch");

	DPRINTF ("HOOK: inotify_add_watch on %s (fd=%d)\n", pathname, fd);
	return func_add_watch (fd, pathname, mask);
}

int inotify_init (void) {
	static int (*func_init) (void) = NULL;

	if (! func_init)
		func_init = (int (*) (void)) dlsym (REAL_LIBC, "inotify_init");

	int fd;
	fd = func_init();

	DPRINTF ("HOOK: inotify_init() returned fd=%d\n", fd);
	return fd;
}

int inotify_rm_watch (int fd, int wd) {
	static int (*func_rm_watch) (int, int) = NULL;

	if (! func_rm_watch)
		func_rm_watch = (int (*) (int, int)) dlsym (REAL_LIBC, "inotify_rm_watch");

	DPRINTF ("HOOK: inotify_rm_watch on fd=%d, wd=%d\n", fd, wd);
	return func_rm_watch (fd, wd);
}