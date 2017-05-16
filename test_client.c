#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

int main(int argc,char **argv)
{
    struct sockaddr_in clientSocketAddr;
	char value[256];
	char len = 255;
	char input[256];
    //char command[] = "set testkey 1";
    //char command_2[] = "get testkey";
    int clientFd = socket(AF_INET,SOCK_STREAM,0);
	
    if (argc < 3)
    {
         printf("argc is less then 3\n");
         return -1;
    }
    if (atoi(argv[2]) < 50000 || atoi(argv[2]) > 65535)
    {
         printf("port between 5000 and 65535\n");
         return -1;
    }	
    socklen_t  socketLen = sizeof(struct sockaddr_in);
    clientSocketAddr.sin_family = AF_INET;
    clientSocketAddr.sin_port   = htons(atoi(argv[2]));
    //serverSocketAddr.sin_addr.s_addr = htonl("172.31.10.169");
    inet_pton(AF_INET,argv[1], &clientSocketAddr.sin_addr);
    if (connect(clientFd,(struct sockaddr*)&clientSocketAddr,socketLen) < 0)
    {
         printf("connect err %s\n",strerror(errno));
         return -1;
    }
    printf("connect pass\n");
	/*if (argc < 2)
	{
	    printf("argc is less then 2\n");
		return -1;
	}
	
    if (send(clientFd,command,sizeof(command),0) < 0)
    {
        printf("send err %s\n",strerror(errno));
    }
    if (send(clientFd,command_2,sizeof(command_2),0) < 0)
    {
        printf("send err2 %s\n",strerror(errno));
    }
	
	if (send(clientFd,argv[1],strlen(argv[1]),0) < 0)
    {
        printf("send err %s\n",strerror(errno));
		return -1;
    }
    
	if (recv(clientFd,value,len,0) < 0)
	{
	    printf("recv err %s\n",strerror(errno));
		return -1;
	}
	else
	{
	    printf("%s\n",value);
	}
	*/
	while(1)
	{
	    printf(">");
		gets(input);
		if (strncmp(input,"quit",4) == 0)
			break;

		if (send(clientFd,input,strlen(input),0) < 0)
	    {
	        printf("send err %s\n",strerror(errno));
			return -1;
	    }
        memset(value,0,sizeof(value));
		
		while(1)
		{
		    int res = recv(clientFd,value,len,0);
			if (res < 0)
			{
			    //printf("recv err %s\n",strerror(errno));
				continue;
				//return -1;
			}
			else if(res == 0)
			{
			    printf("connect err %s\n",strerror(errno));
				return -1;
			}
			else
			{
			    printf("%s\n",value);
				break;
			}
		}
		memset(value,0,sizeof(value));
	}
    return 1;
}
