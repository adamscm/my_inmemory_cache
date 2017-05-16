#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/epoll.h>  
#include <ctype.h>


#define PRINT_ERR printf
#define PRINT_INFO printf

#define PRINT_DEBUG printf
pthread_t g_serverMainThread;


pthread_mutex_t gListenMutex ;


typedef struct tagINMEMNODE{
	char *key;
	char *value;
	unsigned int value_len:16;
	unsigned int total:16;
	unsigned int free:16;
	unsigned short key_len:12;
	unsigned short type:4;
	struct tagINMEMNODE *ptrNext;
}Node;
typedef struct tagHASHNODE{
	Node *ptrNext;
}HashNode;

#define HASH_NODE_NUM 10000

HashNode gHashMap[HASH_NODE_NUM];

char *gSaveCommand = NULL;
unsigned int gWriteSaveComandCur = 0;
unsigned int gReadSaveComandCur = 0;
unsigned int gSaveComandCount = 0;

#define SAVE_COMMAND_NUM 1000
#define SAVE_COMMAND_LEN 1024

#define KEY_MAX_LENGTH     256
#define VALUE_MAX_LENGTH   1024

#define TOTALLENGTH (SAVE_COMMAND_LEN*SAVE_COMMAND_NUM)


#define MULT 31


#define TYPE_SET 1

#define MIN_NODE 16

int g_serverFd = 0;

#define MAXEVENTS 128


char *strupr(char *str){
    char *orign=str;
    for (; *str!='\0'; str++)
        *str = tolower(*str);
    return orign;
}


unsigned int hash(char *p,int len)
{
    unsigned int h = 0;
    while(len)
    {
        h = MULT *h + *p;
		len--;
	    p++;
    }
    return h % HASH_NODE_NUM;
}

unsigned int my_setValue_New(char*key,unsigned int key_len,char*value,unsigned int value_len)
{
    unsigned int hashkey = hash(key,key_len);
	if (gHashMap[hashkey].ptrNext)
	{
		Node *temp = gHashMap[hashkey].ptrNext;
		Node *prev;
		while(temp)
		{
		    if (temp->key_len == key_len)
		    {
		    	if(TYPE_SET == temp->type)
		    	{
		    		if (0 == memcmp(key,temp->key,key_len))
		    			{
		    			    unsigned int total = temp->total;
		    			    if (value_len <= total)
		    			    {
	    			    	    memset(temp->value,0,temp->value_len);
								memcpy(temp->value,value,value_len);
								temp->value_len = value_len;
								temp->free = total - value_len;
		    			    }
							else
							{
								free(temp->value);
								unsigned int size = ((value_len/MIN_NODE+1)*MIN_NODE);
								temp->value = malloc(size);								
								memcpy(temp->value,value,value_len);
								memset(temp->value+value_len,0,size-value_len);
								temp->value_len = value_len;
								temp->total = size;
								temp->free = size - value_len;
							}
		    				return 0;
		    			}
		    	}
				else
				{
					return 3;
				}
		    }
			prev = temp;
			temp = temp->ptrNext;
		}
		if (prev)
		{
			Node *new = (Node*)malloc(sizeof(Node));
			if (new)
			{
			    memset(new,0,sizeof(Node));
				unsigned int size = ((value_len/MIN_NODE+1)*MIN_NODE);
				new->value = malloc(size);
				memcpy(new->value,value,value_len);
				memset(new->value+value_len,0,size-value_len);
				new->value_len = value_len;
				new->total = size;
				new->free = size - value_len;
				new->type = TYPE_SET;

				unsigned int keysize = ((key_len/MIN_NODE+1)*MIN_NODE);
				new->key = malloc(key_len);
				memcpy(new->key,key,key_len);
				new->key_len = key_len;
				
                prev->ptrNext = new;
					
				return 1;
			}
			return 5;
			
		}
	}
	else
	{
		Node *new = (Node*)malloc(sizeof(Node));
		if (new)
		{
		    memset(new,0,sizeof(Node));
			unsigned int size = ((value_len/MIN_NODE+1)*MIN_NODE);
			new->value = malloc(size);
			memcpy(new->value,value,value_len);
			memset(new->value+value_len,0,size-value_len);
			new->value_len = value_len;
			new->total = size;
			new->free = size - value_len;
			new->type = TYPE_SET;

			unsigned int keysize = ((key_len/MIN_NODE+1)*MIN_NODE);
			new->key = malloc(key_len);
			memcpy(new->key,key,key_len);
			new->key_len = key_len;
			
		    gHashMap[hashkey].ptrNext = new;
				
			return 1;
		}
		return 5;
	}
}

char* my_getValue_New(char*key,unsigned int key_len)
{
    unsigned int hashkey = hash(key,key_len);
	if (gHashMap[hashkey].ptrNext)
	{
		Node *temp = gHashMap[hashkey].ptrNext;
		Node *prev;
		while(temp)
		{
		    if (temp->key_len == key_len)
		    {
		    	if(TYPE_SET == temp->type)
		    	{
		    		if (0 == memcmp(key,temp->key,key_len))
		    			{
		    			    char *tempvalue = (char*)malloc(temp->value_len+1);
							memcpy(tempvalue,temp->value,temp->value_len);
							tempvalue[temp->value_len] = '\0';
		    				return tempvalue;
		    			}
		    	}
				else
				{
					return NULL;
				}
		    }
			prev = temp;
			temp = temp->ptrNext;
		}
	}
	else
	{
		return NULL;
	}
}

static int my_insert_savecommand(char*key,unsigned int key_len,char*value,unsigned int value_len)
{
	if (gWriteSaveComandCur >= gReadSaveComandCur)
	{
		unsigned int len = key_len+value_len+8;
		
		if (gWriteSaveComandCur + len <= TOTALLENGTH)
		{
		    PRINT_INFO("write len %d\n",len);
		    *(gSaveCommand+gWriteSaveComandCur) = ((len>>16)&0xff);
			*(gSaveCommand+gWriteSaveComandCur+1) = ((len>>8)&0xff);
			*(gSaveCommand+gWriteSaveComandCur+2) = (len&0xff);
			*(gSaveCommand+gWriteSaveComandCur+3) = TYPE_SET&0xff;
			*(gSaveCommand+gWriteSaveComandCur+4) = ((key_len>>8)&0xff);
			*(gSaveCommand+gWriteSaveComandCur+5) = (key_len&0xff);
			*(gSaveCommand+gWriteSaveComandCur+6) = ((value_len>>8)&0xff);
			*(gSaveCommand+gWriteSaveComandCur+7) = (value_len&0xff);
			memcpy(gSaveCommand+gWriteSaveComandCur+8,key,key_len);
			memcpy(gSaveCommand+gWriteSaveComandCur+8+key_len,value,value_len);
			gWriteSaveComandCur = gWriteSaveComandCur+len;
		}
	    else if (gWriteSaveComandCur < TOTALLENGTH - 4)
	    {
	        if (len - 4 <= gReadSaveComandCur)
	        {
		    	*(gSaveCommand+gWriteSaveComandCur) = ((len>>16)&0xff);
				*(gSaveCommand+gWriteSaveComandCur+1) = ((len>>8)&0xff);
				*(gSaveCommand+gWriteSaveComandCur+2) = (len&0xff);
				*(gSaveCommand+gWriteSaveComandCur+3) = TYPE_SET&0xff;
				*(gSaveCommand) = ((key_len>>8)&0xff);
				*(gSaveCommand+1) = (key_len&0xff);
				*(gSaveCommand+2) = ((value_len>>8)&0xff);
				*(gSaveCommand+3) = (value_len&0xff);
				memcpy(gSaveCommand+4,key,key_len);
				memcpy(gSaveCommand+4+key_len,value,value_len);
				gWriteSaveComandCur = len-4;
	        }
			else
			{
				PRINT_ERR("write save command full2!!!\n");
			}
	    }
		else
		{
			gWriteSaveComandCur = 0;
			if (len <= gReadSaveComandCur)
			{
				*(gSaveCommand+gWriteSaveComandCur) = (char)((len>>16)&0xff);
				*(gSaveCommand+gWriteSaveComandCur+1) = (char)((len>>8)&0xff);
				*(gSaveCommand+gWriteSaveComandCur+2) = (char)(len&0xff);
				*(gSaveCommand+gWriteSaveComandCur+3) = (char)TYPE_SET&0xff;
				*(gSaveCommand+gWriteSaveComandCur+4) = (char)((key_len>>8)&0xff);
				*(gSaveCommand+gWriteSaveComandCur+5) = (char)(key_len&0xff);
				*(gSaveCommand+gWriteSaveComandCur+6) = (char)((value_len>>8)&0xff);
				*(gSaveCommand+gWriteSaveComandCur+7) = (char)(value_len&0xff);
				memcpy(gSaveCommand+gWriteSaveComandCur+8,key,key_len);
				memcpy(gSaveCommand+gWriteSaveComandCur+8+key_len,value,value_len);
				gWriteSaveComandCur = len;
			}
			else
			{
			    PRINT_ERR("write save command full!!!\n");
			}
		}
	}
	else
	{
	}	
}
int my_resolveCommand(const char *command,const int len,void * clientFd)
{   
    PRINT_DEBUG("Command: %s\n",command);
    char type[16],key[1024],value[1024];
    int i = 0,key_len = 0,value_len = 0;
	int comlen = len;
	memset(type,0,sizeof(type));
	memset(key,0,sizeof(key));
	memset(value,0,sizeof(value));

	while(comlen && *command == ' ')
	{
		command++;
		comlen--;
	}
    while(comlen)
    {
           if (*command != ' ')
           {    
               type[i] = *command;
               i++;
			   comlen--;
			   command++;
           }
           else
           {
               type[i] = '\0';
               if (strncmp(strupr(type),"set",3) == 0)
               {
                    while(*command == ' ')
                    {
                        command++;
						comlen--;
                    }
                    i = 0;
                    while(comlen)
                    {
                        if (comlen && *command != ' ')
                        {
                             key[i] = *command;
                             i++;
                             command++;
							 comlen--;
                        }
                        else
                        {    
                             key_len = i;
							 key[i] = '\0';
                             break;
                        }
                    }
                    while(*command == ' ')
                    {
                          command++;
						  comlen--;
                    }
                    i = 0;
                    while(comlen)
                    {
                         if (comlen && *command != ' ')
                         {
                              value[i] = *command;
                              i++;
                              command++;
							  comlen--;
                         }
                         else
                         {
                              value_len = i;
                              break;
                         }
                    }
					value[i] = '\0';
					value_len = i;
					int res = my_setValue_New(key,key_len,value,value_len);
                    if (0 == res)
					{
						char temp[256];
						sprintf(temp,"update set key:%s,value:%s success!\n",key,value);
						send(*(int*)clientFd,temp,strlen(temp)+1,0);
						my_insert_savecommand(key,key_len,value,value_len);
					}
					else if(1 == res)
					{
						char temp[256];
						sprintf(temp,"set key:%s,value:%s success!\n",key,value);
						send(*(int*)clientFd,temp,strlen(temp)+1,0);
						my_insert_savecommand(key,key_len,value,value_len);
					}
					else
					{
					    char temp[256];
						sprintf(temp,"set key:%s,value:%s failed!\n",key,value);
						send(*(int*)clientFd,temp,strlen(temp)+1,0);
					}
               }
			   else if (strncmp(strupr(type),"get",3) == 0)
               {                             
                    char *value; 
					while(comlen && *command == ' ')
					{
					     command++;
						 comlen--;
					}
					
					i = 0;
					
					while(comlen)
					{
					     if (*command != ' ')
						 {
							 key[i] = *command;
							 i++;
							 comlen--;
							 command++;
						 }
						 else
						 {
						     key_len = i;
							 break;
						 }
					}
					key[i] = '\0';	 
					key_len = i;
					value = my_getValue_New(key,key_len);
					if (value != NULL)
					{
					    char temp[256];
			            snprintf(temp,255,"get key:%s,value:%s\n",key,value);
					    send(*(int*)clientFd,temp,strlen(temp)+1,0);
					}
					else
					{
					    char temp[256];
						sprintf(temp,"not find key:%s\n",key);
					    send(*(int*)clientFd,temp,strlen(temp)+1,0);
					}
               }
			   else
			   {
			        printf("not find type\n");
					char temp[256];
					sprintf(temp,"not find type:%s\n",type);
				    send(*(int*)clientFd,temp,strlen(temp)+1,0);
			   }
               i = 0; 
           }
    }
    return 0;
}

static void* f_workber_function(void* para)
{
	int clientFd = 0;
	struct sockaddr_in clientAddr;
	char revBuf[1024];
	
    PRINT_INFO("f_workber_function %d,%d,%ld\n",*(int *)para,g_serverFd,(pthread_self()));
    int gFdSet[128];
    int gFdNum = 0; 
	socklen_t  clientSockLen = sizeof(struct sockaddr_in);
	memset(gFdSet,0,sizeof(gFdSet));
	
	while(1)
	{   
	     pthread_mutex_lock(&gListenMutex);
	     clientFd = accept(g_serverFd,(struct sockaddr *)&clientAddr,&clientSockLen);
		 pthread_mutex_unlock(&gListenMutex);
	     if (clientFd<0 && gFdNum <= 0)
	     {

		    if (errno==EAGAIN || errno == EWOULDBLOCK)
            {
                usleep(10000);
                continue;
            }
            else
            {
                perror("call accept error");
                break;
            }
	        //PRINT_ERR("accept err:%s\n",strerror(errno));
	     	//continue;
	     }
		 else
         {
             if (clientFd > 0)
             {
                 bool flag = true;
                 for(int i = 1; i < 128; i++)
                 {
                    if (gFdSet[i] == 0)
                    {
		             	gFdSet[i] = clientFd;
						int flags = fcntl(clientFd, F_GETFL, 0);
	    				fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);
						gFdNum++;
						flag = false;
						break;
                    }
                 }
	             
				 if (flag)
				 {
				    PRINT_ERR("connect socket number exceed 128!");
	             }
             }
			 
	     }

		 if (gFdNum > 0)
		 {
		     for (int i = 0; i < 128; i++)
		     	{
		     	    if (gFdSet[i] != 0)
		     	    {
						memset(revBuf,0,1024);
						int len = recv(gFdSet[i],revBuf,1024,0);
						if (len > 0 )
						{
						    PRINT_INFO("thread id %ld\n",pthread_self());
							my_resolveCommand(revBuf,len,(void*)&gFdSet[i]);
						}
						else if(len == 0)
						{
						    close(gFdSet[i]);
							gFdSet[i] = 0;
							gFdNum--;
							PRINT_ERR("connect close:%d",gFdSet[i]);
						}
						else
						{
						    //PRINT_ERR("recv err:%s",strerror(errno));
						}
		     	    }
		     	}
		 }
	}
}

static int my_restore_savecommand()
{
    unsigned int filelen = 0,readlen = 0;
	char *temp = NULL;
	FILE *ptrFile = fopen("backcommand","r");
	if (NULL == ptrFile)
	{
		PRINT_ERR("my_restore_savecommand open file error!\n");
		return -1;
	}
	fseek(ptrFile,0,SEEK_END);
	filelen = ftell(ptrFile);
	fseek(ptrFile,0,SEEK_SET);
	temp = (char*)malloc(10240);
	if (NULL == temp)
	{
		PRINT_ERR("my_restore_savecommand malloc error!\n");
		fclose(ptrFile);
		ptrFile = NULL;
		return -1;
	}
	memset(temp,0,10240);

	bool readend = false;
	PRINT_INFO("file len %d\n",filelen);
	while(filelen && (false == readend))
	{
	    if (filelen >= 10240)
	    {
	    	readlen = 10240;
	    }
		else
		{
			readlen = filelen;
			readend = true;
		}
		readlen = fread(temp,1,readlen,ptrFile);
		if (readlen <= 0)
		{
			PRINT_ERR("my_restore_savecommand read file error!\n");
			fclose(ptrFile);
			ptrFile = NULL;
			free(temp);
			return -1;
		}
		filelen -= readlen;
		unsigned int len = 0;
		int type = 0;
		unsigned int key_len   = 0;
		unsigned int value_len = 0;
		char key[KEY_MAX_LENGTH];
		char value[VALUE_MAX_LENGTH];
		
		for(int i = 0; i < readlen; i++)
		{
			if (temp[i] == ' ')
				continue;
			if (i+4 <= readlen)
			{
				len = temp[i]<<16|temp[i+1]<<8|temp[i+2];
				type = (int)temp[i+3];
			}
			else
			{
				PRINT_ERR("savecommand file error,read len err length not enough %d %d!\n",i,readlen);
				fclose(ptrFile);
				ptrFile = NULL;
				free(temp);
				return 0;
			}

			if (i+len <= readlen)
			{
				key_len   = temp[i+4]<<8|temp[i+5];
				value_len = temp[i+6]<<8|temp[i+7];
				if (key_len > KEY_MAX_LENGTH)
				{
				    PRINT_ERR("read command error,key_len more than KEY_MAX_LENGTH!");
					fclose(ptrFile);
					ptrFile = NULL;
					free(temp);
					return 1;
				}
				if (value_len > VALUE_MAX_LENGTH)
				{
				    PRINT_ERR("read command error,value_len more than VALUE_MAX_LENGTH!");
					fclose(ptrFile);
					ptrFile = NULL;
					free(temp);
					return 1;
				}
				memcpy(key,&temp[i+8],key_len);
				key[key_len] = '\0';
				memcpy(value,&temp[i+8+key_len],value_len);
				value[value_len] = '\0';
				PRINT_INFO("key %s\n",key);
				if (type == TYPE_SET)
				{
				    int res = my_setValue_New(key,key_len,value,value_len);
					if (res == 0)
					{
						PRINT_INFO("set update %s\n",key);
					}
					else if (res == 1)
					{
						PRINT_INFO("set insert %s\n",key);
					}
					else
					{
						PRINT_ERR("set error %s %d\n",key,res);
					}
				}

				i += (len - 1);
			}
			else
			{
				PRINT_ERR("savecommand fileerror,read data err length not enough or length error!\n");
				fclose(ptrFile);
				ptrFile = NULL;
				free(temp);
				return 0;
			}
			
		}
	}
	fclose(ptrFile);
	ptrFile = NULL;
	free(temp);
	return 0;
}


static void* f_workber_save_function(void* para)
{
	FILE *ptrFile  = fopen("backcommand","a+");
	char ptrCommand[SAVE_COMMAND_LEN+1];
    PRINT_INFO("f_workber_save_function\n");
	if (NULL == ptrFile)
	{
		PRINT_ERR("open back file failed!");
		return NULL;
	}
	//fseek(ptrFile, 0L, SEEK_END);
	memset(ptrCommand,0,SAVE_COMMAND_LEN+1);

	bool flag = true;
	
	while(1)
	{ 
	    
		if (gReadSaveComandCur != gWriteSaveComandCur)
		{
			if (NULL == ptrFile)
			{
				ptrFile  = fopen("backcommand","a+");
				if (NULL == ptrFile)
				{
					PRINT_ERR("open back file failed!");
					return NULL;
				}
				flag = true;
				fseek(ptrFile, 0L, SEEK_END);
			}
			  
		    PRINT_INFO("save op %d,%d\n",gReadSaveComandCur,gWriteSaveComandCur);
		    unsigned int first = 0, second = 0,three = 0;
			unsigned int len = 0;
		    if (gReadSaveComandCur <= TOTALLENGTH - 4)
		    {
				len = (*(gSaveCommand+gReadSaveComandCur) & 0xff)<<16 | (*(gSaveCommand+gReadSaveComandCur+1) & 0xff)<<8 |(*(gSaveCommand+gReadSaveComandCur+2) & 0xff);
                gReadSaveComandCur+=4;
			}
			else
			{
				len = (*(gSaveCommand) & 0xff)<<16 | (*(gSaveCommand+1) & 0xff)<<8 | (*(gSaveCommand+2) & 0xff);
			}
				
			PRINT_INFO("command len %d\n",len);
			if (gReadSaveComandCur+len-4 < TOTALLENGTH)
			{
			    if (gReadSaveComandCur+len-4 <= gWriteSaveComandCur )
			    {
					memcpy(ptrCommand,gSaveCommand+gReadSaveComandCur-4,len);
					gReadSaveComandCur = gReadSaveComandCur+len-4;
			    }
				else
				{
					PRINT_ERR("read savecommand error!read more than write!\n");
					//abort();
					break;
				}
			}
			else
			{   
			    if ((len-4) <= gWriteSaveComandCur)
			    {
			        memcpy(ptrCommand,gSaveCommand+gReadSaveComandCur,4);
					memcpy(ptrCommand+4,gSaveCommand,len-4);
				    gReadSaveComandCur = len-4;
			    }
				else
				{
					PRINT_ERR("read savecommand error!abnormal!\n");
					//abort();
					break;
				}
			}

			fwrite(ptrCommand,len,1,ptrFile);
		}
		else
		{
		    if (flag)
		    {   
		        flag = false;
		    	fclose(ptrFile);
				ptrFile = NULL;
		    }
		    //PRINT_INFO("sleep 1\n");
			sleep(1);
		}
		
	}
	return NULL;
}

static void* f_workber_function_epoll(void* para)
{
	int clientFd = 0;
	struct sockaddr_in clientAddr;
	char revBuf[1024];
	
    PRINT_INFO("f_workber_function %d,%d,%ld\n",*(int *)para,g_serverFd,(pthread_self()));
    int gFdSet[128];
    int gFdNum = 0; 
	socklen_t  clientSockLen = sizeof(struct sockaddr_in);
	memset(gFdSet,0,sizeof(gFdSet));

    struct epoll_event event;  
    struct epoll_event* events;
	
	int efd = epoll_create(1024);  
    if(efd==-1)  
    {  
        PRINT_ERR("epoll_create");
        perror("epoll_create");  
        abort();  
    }  
    event.data.fd=g_serverFd;  
	event.events= EPOLLIN | EPOLLET;  
	int s = epoll_ctl(efd, EPOLL_CTL_ADD,g_serverFd,&event);  
	if(s ==-1)  
	{  
	    PRINT_ERR("epoll_ctl err");
	    perror("epoll_ctl");  
	    abort();  
	}  
  
    /* Buffer where events are returned */  
    events=calloc(MAXEVENTS,sizeof event); 
	
	while(1)
	{   
/*
	     //pthread_mutex_lock(&gListenMutex);
	     clientFd = accept(g_serverFd,(struct sockaddr *)&clientAddr,&clientSockLen);
		 //pthread_mutex_unlock(&gListenMutex);
	     if (clientFd<0 && gFdNum <= 0)
	     {

		    if (errno==EAGAIN || errno == EWOULDBLOCK)
            {
                usleep(10000);
                continue;
            }
            else
            {
                perror("call accept error");
                break;
            }
	        //PRINT_ERR("accept err:%s\n",strerror(errno));
	     	//continue;
	     }
		 else
         {
             if (clientFd > 0)
             {
                bool keepalive = true;
				setsockopt(clientFd,SOL_SOCKET,SO_KEEPALIVE,(char *)&keepalive,sizeof(keepalive));
                int flags = fcntl(clientFd, F_GETFL, 0);
	    		fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);
                event.data.fd=clientFd;  
				event.events= EPOLLIN | EPOLLET;  
				int s = epoll_ctl(efd, EPOLL_CTL_ADD,clientFd,&event);  
				if(s ==-1)  
				{  
				    perror("epoll_ctl");  
				    abort();  
				}  
             }
			 
	     }
*/
		 int n = epoll_wait(efd, events, MAXEVENTS,-1);
		 for (int i = 0; i < n; i++)
		 {
		 	struct epoll_event event = events[i];
			if (event.data.fd == g_serverFd)
			{
				clientFd = accept(g_serverFd,(struct sockaddr *)&clientAddr,&clientSockLen);
				if (clientFd >= 0)
				{
				    event.data.fd=clientFd;  
					event.events= EPOLLIN | EPOLLET;  
					int s = epoll_ctl(efd, EPOLL_CTL_ADD,clientFd,&event);  
					if(s ==-1)  
					{  
	                    PRINT_ERR("epoll_ctl err");
						perror("epoll_ctl err");  
					    abort();  
					}
					event.data.fd=g_serverFd;  
					event.events= EPOLLIN | EPOLLET;  
					s = epoll_ctl(efd, EPOLL_CTL_MOD,g_serverFd,&event);
					if(s == -1)  
					{  
	                    PRINT_ERR("epoll_ctl err");
						perror("epoll_ctl err");  
					    abort();  
					}
					PRINT_INFO("accept fd:%d\n",clientFd);
				}
				
			}
			else if (event.events & EPOLLIN == EPOLLIN)
			{
				memset(revBuf,0,1024);
				int socketFd = event.data.fd;
				int len = recv(socketFd,revBuf,1024,0);
				
				if (len > 0 )
				{
				    PRINT_INFO("thread id %ld\n",pthread_self());
					my_resolveCommand(revBuf,len,(void*)&socketFd);
				}
				else if(len == 0)
				{
				    close(socketFd);
					epoll_ctl(efd, EPOLL_CTL_DEL,socketFd,&event);
					PRINT_ERR("connect close:%d",socketFd);
				}
				else
				{
				    //PRINT_ERR("recv err:%s",strerror(errno));
				}
				event.data.fd=socketFd;  
				event.events= EPOLLIN | EPOLLET;  
				int s = epoll_ctl(efd, EPOLL_CTL_MOD,socketFd,&event);  
				if(s ==-1)  
				{  
                    PRINT_ERR("epoll_ctl err");
					perror("epoll_ctl err");  
				    abort();  
				}  
			}
		 }
	}
}

int main(int argc,char **argv)
{       
	int threads = sysconf(_SC_NPROCESSORS_ONLN);
	int totalthreads = sysconf(_SC_NPROCESSORS_CONF);
	PRINT_INFO("CORE Number is:%d\n",threads);
	PRINT_INFO("Total CORE Number is:%d\n",totalthreads);
	
	if (argc < 2)
	{
	     PRINT_ERR("argc is less then 2\n");
	     return -1;
	} 
	if (atoi(argv[1]) < 50000 || atoi(argv[1]) > 65535 )
	{
	     PRINT_ERR("PORT must between 50000 and 65535\n");
	     return -1;
	}

	memset(gHashMap,0,sizeof(HashNode)*HASH_NODE_NUM);

	// pthread_t g_serverMainThread;
	g_serverFd = socket(AF_INET,SOCK_STREAM,0);
	if (g_serverFd == -1)
	{
	      return -1;
	}
	struct sockaddr_in serverAddr;
	memset(&serverAddr,0,sizeof(serverAddr));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port   = htons(atoi(argv[1]));
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	int option=1;
    setsockopt(g_serverFd,SOL_SOCKET,SO_REUSEADDR,(char *)&option,sizeof(option));
	struct linger li;
    li.l_onoff=1;
    li.l_linger=1;
    setsockopt(g_serverFd,SOL_SOCKET,SO_LINGER,(char *)&li,sizeof(li));
	if (-1 == bind(g_serverFd,(struct sockaddr *)&serverAddr,sizeof(serverAddr)))
	{
	        PRINT_ERR("bind socket err!");
	        return -1;
	}

	if (-1 == listen(g_serverFd,15))
	{
	       PRINT_ERR("listen socket err!");
	       return -1;
	}
    int flags = fcntl(g_serverFd, F_GETFL, 0);
    fcntl(g_serverFd, F_SETFL, flags | O_NONBLOCK);

	gSaveCommand = (char *)malloc(SAVE_COMMAND_LEN*SAVE_COMMAND_NUM);
	if (NULL == gSaveCommand)
	{
		PRINT_ERR("malloc gSaveCommand failed!\n");
		return -1;
	}
	memset(gSaveCommand,0,SAVE_COMMAND_LEN*SAVE_COMMAND_NUM);

	if (0 != my_restore_savecommand())
	{
		PRINT_ERR("my_restore_savecommand failed!\n");
		return -1;
	}
/*	
	pthread_mutex_init(&gListenMutex,NULL);
	//f_workber_function(&flags);
	pthread_t workerThread[threads];
	for (int i = 0 ; i < 1 ; i++)
	{
		pthread_create(&workerThread[i],NULL,&f_workber_function_epoll,&i);
	}
	for (int i = 0 ; i < 1 ; i++)
	{
		pthread_join(workerThread[i],NULL);
	}
*/
    
    pthread_mutex_init(&gListenMutex,NULL);
	pthread_t workerSaveThread,workerThread;
	pthread_create(&workerSaveThread,NULL,&f_workber_save_function,NULL);
	pthread_create(&workerThread,NULL,&f_workber_function_epoll,(void*)&g_serverFd);
	pthread_join(workerSaveThread,NULL);
	pthread_join(workerThread,NULL);
	pthread_mutex_destroy(&gListenMutex);
    
	return 0;
}
