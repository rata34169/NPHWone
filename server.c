#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>


#define SERV_PORT 8080 // port use to HTML communicate
#define QUEUE_MAX 8 // max size of request queue
#define MAXLINE  20480 // max size of string that client can send
#define MAXNAME  256 // max size of file path

#define SERVICE_GET 1
#define SERVICE_POST 2

//anonymous structure: to decide file types
struct {
    char *ext;
    char *filetype;
} extensions [] = {
    {"gif", "image/gif" },
    {"jpg", "image/jpeg"},
    {"jpeg","image/jpeg"},
    {"png", "image/png" },
    {"zip", "image/zip" },
    {"gz",  "image/gz"  },
    {"tar", "image/tar" },
    {"htm", "text/html" },
    {"html","text/html" },
    {"exe","text/plain" },
    {0,0} };


void handle_socket(int fd)
{
	int j, file_fd, buflen, len;
    long i, ret;
    char * fstr;
    static char buffer[MAXLINE+1];

	//read request
    ret = read(fd,buffer,MAXLINE);
    if (ret==0||ret==-1)
	{
		printf("error: read(): connect problem.\n");
		return;
	}

    printf("\n\n!!!New Request Arrive!!!\n");
    //tip: set the end of the buffer string to be 0
        if (ret>0&&ret<MAXLINE)
            buffer[ret] = 0;
        else
            buffer[0] = 0;
    printf("%s",buffer);

    //switch the request type
	//support request type
	//	# GET
    //  # POST
    int service_trigger;

    if (!(strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4)))
        service_trigger = SERVICE_GET;
    else if(!(strncmp(buffer,"POST ",5) && strncmp(buffer,"post ",5)))
        service_trigger = SERVICE_POST;
    else
        service_trigger = 0;

    printf("service_trigger: %d\n",service_trigger);

    //deploy service

    //None
    if(!service_trigger)
    {
        printf("Error: service not support...\n");
        return;
    }

    //GET
    else if(service_trigger == SERVICE_GET)
    {
        //replase '\r' '\n' to null
        for (i=0;i<ret;i++) 
            if (buffer[i]=='\r'||buffer[i]=='\n')
                buffer[i] = 0;

        //seperate "GET /index.html" "HTTP/1.0"
        for(i=4;i<MAXLINE;i++) {
            if(buffer[i] == ' ') {
                buffer[i] = 0;
                break;
            }
        }

        //block ".."
        for (j=0;j<i-1;j++)
            if (buffer[j]=='.'&&buffer[j+1]=='.')
            {
                printf("error: request: can't support ..");
            }

        //when client want to get root folder -- return index.html
        if (!strncmp(&buffer[0],"GET /\0",6)||!strncmp(&buffer[0],"get /\0",6) )
            strcpy(buffer,"GET /index.html\0");

        //check file type
        buflen = strlen(buffer);
        fstr = (char *)0;

        for(i=0;extensions[i].ext!=0;i++) {
            len = strlen(extensions[i].ext);
            if(!strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
                fstr = extensions[i].filetype;
                break;
            }
        }

        //file type not support
        if(fstr == 0) {
            fstr = extensions[i-1].filetype;
        }

        // response

        //fail
        if((file_fd=open(&buffer[5],O_RDONLY))==-1)
            write(fd, "Failed to open file", 19);

        //success
        sprintf(buffer,"HTTP/1.0 200 OK\r\nContent-Type: %s\r\n\r\n", fstr);
            write(fd,buffer,strlen(buffer));


        //response file
        while ((ret=read(file_fd, buffer, MAXLINE))>0)
            write(fd,buffer,ret);
    }

    //POST
    else if(service_trigger == SERVICE_POST)
    {
        //find "filename"
        char *tmp = strstr(buffer,"filename");
        if(tmp == 0)
        {
            printf("Error: cant find token \"filename\"\n");
            return;
        }

        char *a, *b;
        a = strstr(tmp,"\"");
        b = strstr(a+sizeof(a),"\"");

        char filename[MAXNAME]={0}, location[MAXNAME]={0};
        strncpy(filename, a+1, b-a-1);
        sprintf(location,"upload/%s",filename);

        //check point: filename location
        printf("filename: %s\nlocation: %s\n",filename ,location);

        //fetch text
        a = strstr(tmp,"\n");
        b = strstr(a+1,"\n");
        a = strstr(b+1,"\n");
        b = strstr(a+1,"---------------------------");

        int upload_fd = open(location,O_CREAT|O_WRONLY|O_TRUNC|O_SYNC,S_IRWXO|S_IRWXU|S_IRWXG);

        write(upload_fd, a+1, b-a-3);

        close(upload_fd);
        printf("upload file: %s\n", filename);

        return;
    }

    return;
}


int main(int argc, char **argv)
{
	int 		listenfd, connfd;
	pid_t 		childpid;
	socklen_t 	clilen;
	struct sockaddr_in cliaddr, servaddr;
	
	//prevent zombie process
	signal(SIGCHLD, SIG_IGN);

	//open socket
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	
	//setting server address argument
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family 		= AF_INET;
	servaddr.sin_addr.s_addr	= htonl(INADDR_ANY);
	servaddr.sin_port			= htons(SERV_PORT);

	//bind
	bind(listenfd,(struct sockaddr_in *) &servaddr, sizeof(servaddr));

	//listen
	listen(listenfd, QUEUE_MAX);

	//accept & processing query
	for( ; ; )
	{
		clilen = sizeof(cliaddr);
		connfd = accept(listenfd, (struct sockaddr_in *) &cliaddr, &clilen);

		//processing query (fork)
		//	# in child process
		if((childpid = fork()) == 0) // child process
		{
			close(listenfd);
			handle_socket(connfd);
			exit(0);
		}

		close(connfd);
	}

}