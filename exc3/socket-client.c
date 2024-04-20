/*
 * socket-client.c
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

#include "socket-common.h"

#define DATA_SIZE       256
#define BLOCK_SIZE      16
#define KEY_SIZE		16  /* AES128 */



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

int main(int argc, char *argv[])
{
	int sd, port;
	ssize_t n;
	char buf[1000];
	char *hostname;
	struct hostent *hp;
	struct sockaddr_in sa;
	struct{
		unsigned char 	in[DATA_SIZE],
						encrypted[DATA_SIZE],
						decrypted[DATA_SIZE];
	} data;


	if (argc != 3) {
		fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
		exit(1);
	}

	hostname = argv[1];
	port = atoi(argv[2]); /* Needs better error checking */

	/* Create TCP/IP socket, used as main chat channel */
	if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	fprintf(stderr, "Created TCP socket\n");
	

	/* Look up remote hostname on DNS */
	if ( !(hp = gethostbyname(hostname))) {
		printf("DNS lookup failed for host %s\n", hostname);
		exit(1);
	}

	/* Connect to remote TCP port */
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	memcpy(&sa.sin_addr.s_addr, hp->h_addr, sizeof(struct in_addr));
	fprintf(stderr, "Connecting to remote host... "); fflush(stderr);

	if (connect(sd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		perror("connect");
		exit(1);
	}
	fprintf(stderr, "Connected.\n");

	
	/* Set key value */
	char str[16];
	sprintf(str, "oslab");
	crypto_key(str, strlen(str));
	/* Initialize cryptodev */
	cryptodev_init();


	/*
	 * Let the remote know we're not going to write anything else.
	 * Try removing the shutdown() call and see what happens.
	
	if (shutdown(sd, SHUT_WR) < 0) {
		perror("shutdown");
		exit(1);
	}
	*/

	while(1){

		bzero(buf, sizeof(buf));
		
		//Prompt To
		printf("To server: ");
		fgets(buf, sizeof(buf), stdin);

		buf[sizeof(buf) - 1] = '\0';


		for(int i = 0; i < DATA_SIZE; i++){
			data.in[i] = buf[i];
			if(i >= strlen(buf))
					break;
		}
		
		/* Encrypt outbox message */
		encrypt(data.in, data.encrypted, sizeof(data.in));

		if (insist_write(sd, data.encrypted, strlen(data.encrypted)) != strlen(data.encrypted)) {
			perror("write");
			exit(1);
		
		}
		
		printf("Waiting for response...\n");

		/* Read answer and write it to standard output */
		for (;;) {
			n = read(sd, buf, sizeof(buf));

			if (n < 0) {
				perror("read");
				exit(1);
			}

			if (n <= 0)
				break;

			
			break;
		}

		for(int i = 0; i < DATA_SIZE; i++)
			data.encrypted[i] = buf[i];

		/* Decrypt outbox message for check */
		decrypt(data.encrypted, data.decrypted, sizeof(data.encrypted));

		//Prompt From Server
		printf("From server: %s",data.decrypted);
		if ((strncmp(data.decrypted, "exit", 4)) == 0) {
			printf("Client Exit...\n");
			break;
		}
	}

	fprintf(stderr, "\nDone.\n");
	return 0;
}
