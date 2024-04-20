#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main (int argc, char **argv){

int fd[2];
pipe(fd);

dup2(fd[0],33);
dup2(fd[1],34);

pipe(fd);

dup2(fd[0],53);
dup2(fd[1],54);

execv("./riddle", argv);

}
