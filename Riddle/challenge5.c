#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define _GNU_SOURCE

int main(int argc, char **argv){

int fd=open("mytext.txt", O_CREAT|O_RDONLY); 
int fd2=99;
dup2(fd,fd2);
execv("./riddle",argv);
}
