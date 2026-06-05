#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>

int main() {
    char path[128];
    char command[128];
    char *args[64];
    while (1)
    {
       if(getcwd(path,sizeof(path)) != NULL) {
            printf("%s$",path);
       }else {
            printf("$");
       }
       fflush(stdout);

       if(fgets(command,sizeof(command),stdin) == NULL) break;
       command[strlen(command)-1] = '\0';        // 去掉换行符
       if(strlen(command) == 0) continue;       // 空回车

       int i = 0;
       args[i] = strtok(command," ");
       while(args[i] != NULL) {
           i++;
           args[i] = strtok(NULL," ");
       }
    }
    return 0;
}