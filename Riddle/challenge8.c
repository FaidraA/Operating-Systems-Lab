#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

int main(void){

    int fd[1024];
    for (int i = 0 ; i < 99 ; i++){
        char filename[] = "bf";
        char buffer[10], suffix[10] = "0";
        snprintf(buffer, sizeof(buffer), "%d", i);
        if(i < 10) 
            strcat(suffix,buffer);
        else
            strcpy(suffix, buffer);
        printf("%s\n", suffix);
        strcat(filename, suffix);
        printf("%s\t%s\n---\n", filename, suffix);
        fd[i] = open(filename, O_RDWR|O_CREAT, 0644);
        pwrite(fd[i], "footer", 6, 1073741824);
        close(fd[i]);
        filename[0] = '\0';
    }
   
    return 0;
}