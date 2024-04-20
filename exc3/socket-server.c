/*
 * socket-server.c
 * Simple TCP/IP communication using sockets
 *
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 */

/*
 * tcpdump -i lo -vvv port 35001 or port 47434 and host 127.0.0.1
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

#include "socket-common.h"
#include <sys/select.h>

#define DATA_SIZE       256
#define BLOCK_SIZE      16
#define KEY_SIZE		16  /* AES128 */

#define MAX(a, b) ((a) > (b) ? (a) : (b))


/* Insist until all of the data has been written */
ssize_t insist_write(int fd, const void *buf, size_t cnt)
{
	ssize_t ret;
	size_t orig_cnt = cnt;
	
	while (cnt > 0) {
	        ret = write(fd, buf, cnt);
	        if (ret < 0)
	                return ret;
	        buf += ret;
	        cnt -= ret;
	}

	return orig_cnt;
}

/* Insist until all of the data has been read */
ssize_t insist_read(int fd, void *buf, size_t cnt)
{
        ssize_t ret;		
        size_t orig_cnt = cnt;

        while (cnt > 0) {
                ret = read(fd, buf, cnt);
                if (ret < 0)
                        return ret;
                buf += ret;
                cnt -= ret;
        }

        return orig_cnt;
}

/* initialize cryptodev api */
int cryptodev_init(){

	cfd = open("/dev/crypto", O_RDWR);
	if (cfd < 0) {
		perror("open(/dev/crypto)");
		return 1;
	}

	memset(&sess, 0, sizeof(sess));
	memset(&cryp, 0, sizeof(cryp));

	
	/*
	 * Get crypto session for AES128
	 */
	sess.cipher = CRYPTO_AES_CBC;
	sess.keylen = KEY_SIZE;
	sess.key = key;
		printf("Key set to: %s\n", key);

	if (ioctl(cfd, CIOCGSESSION, &sess)) {
		perror("ioctl(CIOCGSESSION)");
		return 1;
	}

	for(int i = 0; i < BLOCK_SIZE; i++)
		iv[i] = 0x00;

	cryp.ses = sess.ses;
	cryp.iv = iv;

	printf("Cryptodev initialized! \n");


	return 0;
}

/* Set crypto key */

void crypto_key(char * str, int len){

	int i;
	for(i = 0; i < len; i++)
		key[i] = str[i];
	for(; i < KEY_SIZE; i++)
		key[i] = 0x00;
}

/*Function for encrypting data */
int encrypt(unsigned char * message, unsigned char * encrypted_message, int len){
	
	cryp.ses = sess.ses;
	cryp.iv = iv;
	cryp.len = len;
	cryp.src = message;
	cryp.dst = encrypted_message;
	cryp.op = COP_ENCRYPT;

	if (ioctl(cfd, CIOCCRYPT, &cryp)) {
		perror("ioctl(CIOCCRYPT)");
		return 1;
	}
	return 0;
}

/*Function for decrypting data */
int decrypt(unsigned char * encrypted_message, unsigned char * decrypted_message, int len){
	
	cryp.len = len;
	cryp.src = encrypted_message;
	cryp.dst = decrypted_message;
	cryp.op = COP_DECRYPT;

	if (ioctl(cfd, CIOCCRYPT, &cryp)) {
		perror("ioctl(CIOCCRYPT)");
		return 1;
	}
	return 0;
}



int main(void)
{
	unsigned char buf[1000];
	char addrstr[INET_ADDRSTRLEN];
	int sd, newsd;
	ssize_t n;
	socklen_t len;
	struct sockaddr_in sa;
	struct{
		unsigned char 	in[DATA_SIZE],
						encrypted[DATA_SIZE],
						decrypted[DATA_SIZE];
	} data;

	
	/* Make sure a broken connection doesn't kill us */
	signal(SIGPIPE, SIG_IGN);

	/* Create TCP/IP socket, used as main chat channel */
	if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	fprintf(stderr, "Created TCP socket\n");

	/* Bind to a well-known port */
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(TCP_PORT);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		perror("bind");
		exit(1);
	}
	fprintf(stderr, "Bound TCP socket to port %d\n", TCP_PORT);

	/* Listen for incoming connections */
	if (listen(sd, TCP_BACKLOG) < 0) {
		perror("listen");
		exit(1);
	}

	/* Set key value */
	char str[16];
	sprintf(str, "oslab");
	crypto_key(str, strlen(str));
	/* Initialize cryptodev */
	cryptodev_init();

	/* Loop forever, accept()ing connections */
	for (;;) {
		fprintf(stderr, "Waiting for an incoming connection...\n");

		/* Accept an incoming connection */
		len = sizeof(struct sockaddr_in);
		if ((newsd = accept(sd, (struct sockaddr *)&sa, &len)) < 0) {
			perror("accept");
			exit(1);
		}
		if (!inet_ntop(AF_INET, &sa.sin_addr, addrstr, sizeof(addrstr))) {
			perror("could not format IP address");
			exit(1);
		}
		fprintf(stderr, "Incoming connection from %s:%d\n",
			addrstr, ntohs(sa.sin_port));

		/* We break out of the loop when the remote peer goes away */
		for (;;) {
			
			fd_set inset;
        		int maxfd;
			
        		FD_ZERO(&inset);                
        		FD_SET(STDIN_FILENO, &inset);   
        		FD_SET(sd, &inset);         

			maxfd = MAX(STDIN_FILENO, sd) + 1;
	
			int n = select(maxfd, &inset, NULL, NULL, NULL);

			bzero(buf, sizeof(buf));
			n = read(newsd, buf, sizeof(buf));
			
			
			if (n <= 0) {
				if (n < 0)
					perror("read from remote peer failed");
				else
					fprintf(stderr, "Peer went away\n");
				break;
			}
			
			for(int i = 0; i < DATA_SIZE; i++){
				data.encrypted[i] = buf[i];
				if(i >= strlen(buf))
					break;

			}

			decrypt(data.encrypted, data.decrypted, sizeof(data.encrypted));

			//Prompt From client:
			printf("From client: %s", data.decrypted);
			

			if ((strncmp(data.decrypted, "exit", 4)) == 0) {
				printf("Client Exit...\n");
				ioctl(cfd, CIOCFSESSION, &sess);
				break;
			}
			bzero(buf, sizeof(buf));

			//Prompt To client:
			printf("To client: ");

			fgets(buf, sizeof(buf), stdin);
			
			buf[sizeof(buf) - 1] = '\0';

			
			for(int i = 0; i < DATA_SIZE; i++)
				data.in[i] = buf[i];

			encrypt(data.in, data.encrypted, sizeof(data.in));

			if (insist_write(newsd, data.encrypted, strlen(data.encrypted)) != strlen(data.encrypted)) {
				perror("write to remote peer failed");
				break;
			}
			printf("Waiting for response...\n");

		}
		/* Make sure we don't leak open files */
		if (close(newsd) < 0)
			perror("close");
	}

	/* This will never happen */
	return 1;
}

