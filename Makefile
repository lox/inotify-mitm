
CFLAGS=-fdiagnostics-color=auto -Wall -Wextra -Wwrite-strings -std=c99

all: inotify_hook.so read_listen

inotify_hook.so: inotify_hook.o listen.o jsmn.o
	gcc $(CFLAGS) -shared -o inotify_hook.so inotify_hook.o listen.o jsmn.o -ldl

inotify_hook.o: inotify_hook.c listen.h debug.h
	gcc $(CFLAGS) -fdiagnostics-color=auto -Wall -Wextra -Wwrite-strings -fPIC -c inotify_hook.c

listen.o: listen.c listen.h
	gcc $(CFLAGS) -fdiagnostics-color=auto -Wall -Wextra -Wwrite-strings -fPIC -c listen.c

jsmn.o: jsmn/jsmn.h
	gcc $(CFLAGS) -fdiagnostics-color=auto -Wall -Wextra -Wwrite-strings -fPIC -c jsmn/jsmn.c

read_listen: read_listen.c listen.o jsmn.o
	gcc $(CFLAGS) -fdiagnostics-color=auto -o read_listen read_listen.c listen.o jsmn.o -I.

clean:
	rm -f *.so *.o
