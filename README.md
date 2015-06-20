
inotify mitm
============

This is an experiment in trying to hook inotify calls so that they can be proxied to a network relay like
https://github.com/guard/listen#forwarding-file-events-over-tcp.

Example
---------------------

```bash
make all
LD_PRELOAD=$PWD/inotify_hook.so inotifywait -m -r -e modify,attrib,close_write,move,create,delete /tmp
```

Then in another shell:

```bash
touch /tmp/llamas
```

References
-------

 - http://linux.die.net/man/7/inotify
 - http://linux.die.net/man/2/read
 - http://stackoverflow.com/questions/5589136/copy-struct-to-buffer
 - https://rafalcieslak.wordpress.com/2013/04/02/dynamic-linker-tricks-using-ld_preload-to-cheat-inject-features-and-investigate-programs/
 - https://raw.githubusercontent.com/poliva/ldpreloadhook/master/hook.c
 - https://readmes.numm.org/init/upstart/init/tests/wrap_inotify.c