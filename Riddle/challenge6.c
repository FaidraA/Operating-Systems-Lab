#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv){
	
	
	
	char * myfifo = "play";

	mkfifo(myfifo, 0666);
	

	int fd = open(myfifo, O_RDWR);
	int fd_ = open(myfifo, O_RDWR);
	int fd__ = open(myfifo, O_RDWR);
	int fd___ = open(myfifo, O_RDWR);
	
	int fd2 = 33;
	int fd_2 = 34;
	int fd__2 = 53;
	int fd___2 = 54;

	dup2(fd, fd2);
	dup2(fd_, fd_2);
	dup2(fd__, fd__2);
	dup2(fd___, fd___2);
	pwrite(fd, "pong", 4, 0);
	pwrite(fd__, "ping", 4, 0);
	execv("./riddle", argv);
	/*
	while (1){
		;
	}
	*/
	return 0;
}
