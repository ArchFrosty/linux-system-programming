#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
 
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
 
#define MAX_MSG_LENGTH 512
 
//Author: Igor Veres
char buf[512];
struct sockaddr_un adr;
int s;
char name[] = "/tmp/socket";
int MAX_CONNECTION_TIME = 512; //in seconds

//---------------------------------------------------First launch server with -s and then client with -c ------------------------------------------------
 
 
int execCommand(char* command)
{
 
    if(strcmp(command, "help") == 0)
    {
        printf("info : display date, time, machine name\n");
        printf("run : print all lines in file beginning with capital letter and number of lines printed, usage : run pathtofile\n");
        printf("halt : stop the program\n");
        printf("quit : disconnect\n");
        printf("to get entire communication with server, sometimes enter key has to be pressed multiple times\n");
        printf("after each command, enter key has to be pressed twice\n");
        printf("program intercepts ctrl+c signal and executes halt command\n");
        printf("if command is not recognized, it is passed to the shell\n");
    }
    else if(strcmp(command, "info") == 0)
    {
        time_t time = 0;
        struct utsname *name;
        name = malloc(390);//utsname struct consists of 5 strings, each of length 65
        asm("xor %%ebx, %%ebx; mov $13, %%eax; int $0x80; movl %%eax, %0" : "=r" ( time ) );//ebx register needs to be null and then call time() syscall  which returns time in seconds since the Epoch
        asm("movl %0, %%ebx; mov $122, %%eax; int $0x80" : : "r" ( name ) );//address of name buffer is put into ebx, into which the new_uname syscall writes kernel info
        struct tm *parsedTime;
        parsedTime = localtime(&time);//parse time in seconds since the Epoch into local date and time
        printf("Current local time and date is: %s",asctime(parsedTime));
        printf("Machine name: %s\n", name->nodename);//second string called nodename has the machine name
 
    }
    else if(strcmp(command, "halt") == 0)
    {
        printf("Halt command, exiting program \n");
        exit(0);
    }
    else if(strcmp(command, "quit") == 0)
    {
 
    }
    else if(strncmp(command, "run", 3) == 0)
    {
        command = command + 4;
        char line[512];
        FILE *fp;
        fp = fopen(command, "r");
        int count =0;
        while(fgets(line, 511, fp))
        {
            if(line[0] > 40 && line[0] < 91)
            {
                count++;
                printf("%s", line);
            }
        }
        printf("Number of lines beggining with capital letters: %d\n", count);
    }
    else
    {
        //printf("passing unrecognized command to shell: %s\n", command);
        system(command);
    }
    return 0;
}
 
void sig_handler(int signo)//signal handler
{
 
    if(signo == SIGINT)
    {
        execCommand("halt");
    }
}
 
int startClient()
{
    s = socket(PF_LOCAL, SOCK_STREAM, 0);
    memset(&adr, 0, sizeof(adr));
    adr.sun_family = AF_LOCAL;
    strcpy(adr.sun_path, name);
    connect(s,(struct sockaddr*)&adr, sizeof(adr));
    while(1)
    {
        while(fgets(buf, MAX_MSG_LENGTH-1, stdin))
        {
            if(strcmp(buf, "halt") == 0)
            {
                execCommand("halt");
            }
            else if(strcmp(buf, "quit") == 0)  //for client quit is the same as halt
            {
                execCommand("halt");
            }
            buf[strlen(buf)-1]=0;//remove carriage return otherwise the commands wont match
            write(s, buf, strlen(buf));
            int readsize ;
            ioctl(s, FIONREAD, &readsize);//how much data is in socket to read
            char *string;
            string = malloc(readsize);
            read(s,string,readsize);
            printf("%.*s\n",readsize, string);
 
        }
    }
}
 
int startServer()
{
 
    int s=socket(AF_LOCAL,SOCK_STREAM,0);
 
    memset(&adr,0,sizeof(struct sockaddr_un));
 
    adr.sun_family = AF_LOCAL;
    strcpy(adr.sun_path,"/tmp/socket");
 
    bind(s,(struct sockaddr*)&adr,sizeof(struct sockaddr_un));
    listen(s,5);
    printf("\n");
    int sockets[512];
    char active[512];
    time_t timeInactive[512];
    memset(active,0,512);
    memset(timeInactive,0,512);
    while(1)
    {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(0,&set);
        FD_SET(s,&set);
 
        int max=s;
        int i;
        for(i=0; i<512; i++)if(active[i])
            {
                FD_SET(sockets[i],&set);
                if(max<sockets[i])max=sockets[i];
            }
 
        select(max+1,&set,NULL,NULL,NULL);
 
        if(FD_ISSET(0,&set))
        {
            memset(buf,0,512);
            fgets(buf,511,stdin);
            for(i=0; i<512; i++)if(active[i])write(sockets[i],buf,strlen(buf));
        }
 
        if(FD_ISSET(s,&set))
        {
            for(i=0; i<512; i++)if(!active[i])break;
            sockets[i]=accept(s,NULL,NULL);
            active[i]=1;
        }
        for(i=0; i<512; i++)if(active[i]&&FD_ISSET(sockets[i],&set))
            {
                memset(buf,0,512);
                int r=read(sockets[i],buf,512);
                if(r==0)
                {
                    close(sockets[i]);
                    active[i]=0;
                }
                else
                {
                    dup2(sockets[i], STDOUT_FILENO);
                    dup2(sockets[i], STDERR_FILENO);
                    if(strcmp(buf, "halt") == 0)
                    {
                        int i;
                        for(i=0; i<512; i++)if(active[i])close(sockets[i]);
                        close(s);
                        execCommand("halt");
                    }
                    else if(strcmp(buf, "quit") == 0)
                    {
                        int i;
                        for(i=0; i<512; i++)if(active[i])close(sockets[i]);
                        close(s);
                    }
                    execCommand(buf);
                }
            }
    }
    return 0;
}
 
int main(int argc, char** argv, char** en)
{
    if(signal(SIGINT, sig_handler) == SIG_ERR)
    {
        printf("\n Cant catch SIGINT !!! \n");
    }
 
    if(argc > 1)
    {
        if(strcmp(argv[1], "-s") == 0)
        {
            remove("/tmp/socket");
            startServer();
        }
        else if(strcmp(argv[1], "-c") == 0)
        {
            startClient();
        }
    }
    else
    {
        while(1)
        {
            fgets(buf, MAX_MSG_LENGTH-1, stdin);
            buf[strlen(buf)-1]=0;
            execCommand(buf);
        }
    }
    return 0;
}