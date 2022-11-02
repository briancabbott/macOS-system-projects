#ifdef __linux__
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/types.h>        /* for __u* and __s* typedefs */
#include <linux/socket.h>        /* for "struct sockaddr" et al    */
#include <linux/if_alg.h>
#endif
