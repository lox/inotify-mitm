all:
	gcc -Wall -Wextra -Wwrite-strings -fPIC -c -o inotify_hook.o inotify_hook.c
	gcc -Wall -Wextra -Wwrite-strings -shared -o inotify_hook.so inotify_hook.o -ldl

clean:
	rm -f *.so *.o