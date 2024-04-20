/*
 * socket-common.h
 *
 * Simple TCP/IP communication using sockets
 *
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 */

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <crypto/cryptodev.h>


#include <arpa/inet.h>
#include <netinet/in.h>

#ifndef _SOCKET_COMMON_H
#define _SOCKET_COMMON_H

/* Compile-time options */
#define TCP_PORT    35001
#define TCP_BACKLOG 5

/* Settings for data encryption */
#define BLOCK_SIZE      16
#define KEY_SIZE  16  /* AES128 */

struct session_op sess;
struct crypt_op cryp;
__u8 iv[BLOCK_SIZE];
__u8 key[KEY_SIZE];
int cfd;


#endif /* _SOCKET_COMMON_H */

