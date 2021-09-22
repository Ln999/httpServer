#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<sys/epoll.h>
#include<sys/socket.h>
#include<arpa/inet.h>

#define PORT 8000
#define OPEN_MAX 1024

void err(const char*);

int get_line(int,char*,int);

void http_response(int,char*);

const char*file_type(const char*);

void do_read(int,int);

void send_http_head(int,const char*,long);

void send_file(int,const char*);

void epoll_work(int);

void disconnect(int,int);

void sock_init(int*);

void err(const char*msg){
    printf("%s",msg);
    exit(1);
}

int get_line(int sockfd,char*buffer,int len){
    int i=0;
	char c='\0';
	int n=0;

	while((i<len-1)&&(c!='\n')){
		n=recv(sockfd,&c,1,0);
		if(n>0){
			if(c=='\r'){
				/*拷贝读一次*/
				n=recv(sockfd,&c,1,MSG_PEEK);
				if((n>0)&&(c=='\n')){
					recv(sockfd,&c,1,0);
				}else{
					c='\n';
				}
			}
			buffer[i]=c;
			i++;
		}else{
			c='\n';
		}
	}
	buffer[i]='\0';

	if(n==-1){
		i=n;
	}
	return i;
}

void http_response(int sockfd,char*head_line){
    char method[10];
    char path[1024];
    char protocol[10];
    printf("---%s",head_line);
    sscanf(head_line,"%[^ ] %[^ ] %[^ ]",method,path,protocol);
    printf("method=%s \n,path=%s \n,protocol=%s \n",method,path,protocol);
    
    char*file="./index.html";//指定当前目录的index.html文件
    // printf("file path %s \n",file);
    //获取文件属性
    struct stat st;
    int ret=stat(file,&st);
    printf("ret==%d",ret);
    if(ret==-1){
        err("error page no such file 002");//
    }
    
    //判断是否是目录
    // if(S_ISDIR(st.st_mode));
    if(S_ISREG(st.st_mode)){
        send_http_head(sockfd,file_type(file),st.st_size);
        send_file(sockfd,file);
    }


    

}

const char*file_type(const char*file){
    char*dot;
    dot=strrchr(file,'.');//查找.在file中最后出现的位置
    if(dot==NULL)
		return "text/plain; charset=utf-8";
	if(strcmp(dot,".html")==0||strcmp(dot,"htm")==0)
		return "text/html; charset=utf-8";
	// if(strcmp(dot,".jpg")==0||strcmp(dot,"jpeg")==0)
	// 	return "image/jpeg";
	// if(strcmp(dot,".gif")==0)
	// 	return "image/gif";
	// if(strcmp(dot,".png")==0)
	// 	return "image/png";
	// if(strcmp(dot,".css")==0)
	// 	return "text/css";
	// if(strcmp(dot,".wav")==0)
	// 	return "audio/wav";
	// if(strcmp(dot,".mp3")==0)
	// 	return "audio/mpeg";
	// if(strcmp(dot,".avi")==0)
	// 	return "video/x-msvideo";
	return "text/plain; charset=utf-8";
}

void do_read(int sockfd,int epfd){
    char head_line[1024];
    int len=get_line(sockfd,head_line,sizeof(head_line));
    if(len==0){
        printf("%d is closed",sockfd);
        disconnect(sockfd,epfd);
    }
    else if(len<0){
        err("read error");
        disconnect(sockfd,epfd);
    }
    else{
        //清空缓冲区数据
        while (1){
            char buf[1024];
            len=get_line(sockfd,buf,sizeof(buf));
            if(buf[0]=='\n'){
                break;
            }
            else if(len==-1){
                break;
            }
        }
    }
    printf("Message of http head %s",head_line);
    //获取http请求方式
    if(strncasecmp("get",head_line,3)==0){
        http_response(sockfd,head_line);
    }
    disconnect(sockfd,epfd);
}

void send_http_head(int sockfd,const char*file_type,long len){
    char buffer[1024];
    const char*response_head="HTTP/1.1 200 OK \r\n";
    sprintf(buffer,"%s",response_head);
    send(sockfd,buffer,strlen(buffer),0);
    sprintf(buffer,"Conten-Type:%s \r\n",file_type);
    send(sockfd,"\r\n",2,0);
}

void send_file(int sockfd,const char*file_name){
    int n=0;
	int ret=0;
	char buf[BUFSIZ]={0};

	int fd=open(file_name,O_RDONLY);
	if(fd==-1){
		err("error page no such file ");
	}

	while((n=read(fd,buf,sizeof(buf)))>0){
		ret=send(sockfd,buf,n,0);
		}
}

void epoll_work(int sfd){
    //4,创建epoll监听树
   int epfd= epoll_create(OPEN_MAX);
    struct epoll_event ep_node;//事件节点
    struct epoll_event ep_arr[OPEN_MAX];//事件数组

    //5,初始化epoll树
    ep_node.events=EPOLLIN;
    ep_node.data.fd=sfd;
    epoll_ctl(epfd,EPOLL_CTL_ADD,sfd,&ep_node);

    //6,通信文件描述符信息
    int cfd;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_size;

    //7,处理epoll树事件
    int count;
    int len;
    char buffer[1024];
    int sockfd;
    while (1)
    {
        count=epoll_wait(epfd,ep_arr,OPEN_MAX,-1);
        if(count==-1){
            err("epoll_wait error");
        }
        for(int i=0;i<count;i++){
            sockfd=ep_arr[i].data.fd;
            if(sockfd==sfd){
                //连接请求
                clnt_size=sizeof(clnt_addr);
                cfd=accept(sockfd,(struct sockaddr*)&clnt_addr,&clnt_size);
                if(cfd==-1){
                    err("accept error");
                }
                //添加连接请求到epoll树
                ep_node.events=EPOLLIN;
                ep_node.data.fd=cfd;
                epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&ep_node);
            }
            else{
                do_read(sockfd,epfd);
            }
        }
    }
}

void disconnect(int sockfd,int epfd){
    epoll_ctl(epfd,EPOLL_CTL_DEL,sockfd,NULL);
    close(sockfd);
}

void sock_init(int*sfd){
    //1,建立socket连接
    *sfd=socket(AF_INET,SOCK_STREAM,0);
    if(*sfd==-1){
        err("socket error");
    }

    //2,绑定文件描述符和地址
    struct sockaddr_in serv_addr;
    bzero(&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(PORT);
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    int ret=bind(*sfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr));
    if(ret==-1){
        err("bind error");
    }

    //3,设置监听数量
    ret=listen(*sfd,128);
    if(ret==-1){
        err("listen error");
    }
}

int main(){
    int sfd;
    sock_init(&sfd);
    epoll_work(sfd);
    return 0;

}