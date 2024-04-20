#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>

int main(int argc, char **argv){

char* buf[1];
buf[0] = argv[2];
int fd = open(argv[1], O_RDWR);

lseek(fd,111, SEEK_SET);  
write(fd,&buf,1);

//char* letter=(char*)mmap(NULL,4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
//letter[111]= *argv[2];

}
