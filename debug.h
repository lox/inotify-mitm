
#define DEBUG

#ifdef DEBUG
# define DPRINTF(format, args...)    fprintf(stderr, format, ## args)
#else
# define DPRINTF(format, args...)    do {} while (0)
#endif
