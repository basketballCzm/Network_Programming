#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLINE   10
#define OPEN_MAX  100
#define LISTENQ   20
#define SERV_PORT 5555
#define INFTIM    1000

void setnonblocking(int sock){
	int opts;
	opts = fcntl(sock, F_GETFL);

	if(opts < 0){
		perror("fcntl(sock, GETFL)");
		exit(1);
	}

	opts = opts | O_NONBLOCK;

	if(fcntl(sock, F_SETFL, opts) < 0){
		perror("fcntl(sock, F_SETFL, opts)");
		exit(1);
	}
}

//epoll_create,epoll_ctl,epoll_wait
int main(int argc, char *argv[]){
	printf("epoll socket begins.\n");
	int i, maxi, listenfd, connfd, sockfd, epfd, nfds;
	//read的返回值
	ssize_t  n;
	//接收数据的buf
	char line[MAXLINE];
	socklen_t client;

	struct epoll_event ev, events[20];

	epfd = epoll_create(256);

	struct sockaddr_in clientaddr;
	struct sockaddr_in serveraddr;

	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	setnonblocking(listenfd);

	ev.data.fd = listenfd;
	//设置事件类型和工作方式
	ev.events = EPOLLIN | EPOLLET;
	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	char *local_addr = "192.168.";
	//主机字节序转化为网络字节序
	inet_aton(local_addr, &(serveraddr.sin_addr));
	serveraddr.sin_port = htons(SERV_PORT);

	bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));

	listen(listenfd, LISTENQ);
	maxi = 0;

	while(1){
		//等待事件发生
		nfds = epoll_wait(epfd, events, 20, 500);
		for(i = 0; i < nfds; i++){
			//检测到用户连接
			if(events[i].data.fd == listenfd){
				printf("accept connection, fd is %d\n", listenfd);
				//接收客户端的请求
				connfd = accept(listenfd, (struct sockaddr*)&clientaddr, &client);
				if(connfd < 0){
					perror("connfd < 0");
					exit(1);
				}

				//设置为非阻塞状态
				setnonblocking(connfd);

				//网络字节序转化为主机字节序
				char *str = inet_ntoa(clientaddr.sin_addr);
				printf("connect from %s:%d\n", str, clientaddr.sin_port);

				ev.data.fd = connfd;
				ev.events = EPOLLIN | EPOLLET;
				epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
			}else if(events[i].events & EPOLLIN){
				if((sockfd = events[i].data.fd) < 0) continue;
				if((n = read(sockfd, line, MAXLINE)) < 0){
					if(errno == ECONNRESET){
						close(sockfd);
						events[i].data.fd = -1;
					}else{
						printf("readline error!");
					}
				}else if(0 == n){
					close(sockfd);
					events[i].data.fd = -1;
				}

				line[n] = '\0';
				printf("received data : %s\n", line);

				ev.data.fd = sockfd;
				ev.events = EPOLLOUT | EPOLLET;
				epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
			}else if(events[i].events & EPOLLOUT){
				if((sockfd = events[i].data.fd) < 0){
					continue;
				}
				int ret = 0;
				//sockfd = events[i].data.fd;
				if(ret = write(sockfd, line, n) != n){
					printf("error writing to sockfd!\n");
					break;
				}

				printf("writen data: %s\n",line);

				//设置用于读的文件描述符和事件
				ev.data.fd = sockfd;
				ev.events = EPOLLIN | EPOLLET;
				//重新注册事件
				epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
			}
		}
	}

	free(events);
	close(epfd);
	return 0;
}

/*
在不同平台上，其具有不同的定义：

sparc 64 bit 
typedef unsigned long          __kernel_size_t;
typedef long                   __kernel_ssize_t;

sparc 32 bit
typedef unsigned int           __kernel_size_t;
typedef int                    __kernel_ssize_t;

一般在对于缓冲区大小等于非负值的长度时一般使用size_t，而对于一些返回值可能为负数的函数，返回值使用ssize_t

*/