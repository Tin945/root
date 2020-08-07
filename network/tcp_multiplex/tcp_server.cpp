#include "tcp.h"

void clientHandle(int fd);
void sigalHandle(int signo);
int clientCount = 0;	//接入的客户端数量
/* 问题1:客户端的IP与Port相同情况下如何区分客户端 */
/* 问题2:如何同时对每个客户端说 */
int main()
{
	int socket_fd;
	struct sockaddr_in sin;
	signal(SIGCHLD, sigalHandle);
	/* 1.建立socket通信 */
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd == -1){
		perror("socket");
		exit(-1);
	}
	
	/* 优化4:允许绑定地址重用 */
	int b_reuse = 1;
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &b_reuse, sizeof(int));
	
	/* 2.绑定ip与port */
	bzero(&sin, sizeof(sin));			//结构体清零
	sin.sin_family = AF_INET;			//协议域IPv4
	sin.sin_port = htons(SERV_PORT);	//端口号转为网络字节序

	/* 优化1:让服务器程序能绑定在任意IP上 */
	sin.sin_addr.s_addr = htonl(INADDR_ANY);	//IP转网络字节序
	
	//需要将ipv4结构体强转为通用结构体
	if (bind(socket_fd, (struct sockaddr*)&sin, sizeof(sin)) < 0){	
		perror("bind");
		exit(-1);
	}
	
	/* 3.listen监听, 把主动套接字变为被动套接字*/
	if (listen(socket_fd, BACKLOG) < 0){
		perror("listen");
		exit(-1);
	}
	printf("直播间还没有人来......\n");
	/* 4.accept阻塞等待客户端连接请求 */
	/* 优化2:通过程序获取刚连接的socket的客户端的IPC地址与端口号 */
	struct sockaddr_in cin;
	int client_fd;
	socklen_t addrlen = sizeof(cin);

	/* 优化3:利用多进程/线程处理建立好连接的客户端数据 */
	/* 每多一个客户端接入就多分配一个socket去处理 */
	while (1){
		pid_t pid;
		/* accept阻塞等待新的client连接,cin会被赋值为客户端的信息,ip、端口等 */
		client_fd = accept(socket_fd, (struct sockaddr *)&cin, &addrlen);	
		if (client_fd < 0){
			//perror("accept");
			break;
		}
		pid = fork();	//创建一个子进程用于处理已建立连接的客户端交换数据
		if (pid < 0){
			perror("fork");
			continue;
		}
		else if (0 == pid){	//子进程
			close(socket_fd);	//子进程不需要原socket
			clientHandle(client_fd);
		}
		else{	//父进程
			//父进程不需要用到新的socket
			close(client_fd);
		}
		/* 将网络字节序的ip转为点分形式的字符串 */
	}
	close(socket_fd);
	return 0;
}
/* 处理客户端的进程处理 */
void clientHandle(int fd)	//参数为newfd
{
	int ret;
	pid_t reply_pid;
	char buf[BUFFSIZE];
	printf("< %d > 进入直播间!\n", getpid());
	reply_pid = fork();
	if (fork < 0){
		perror("fork");
		exit(-1);
	}
	else if (0 == reply_pid){	//子进程-应答
		while (1){
			bzero(buf, sizeof(buf));	/* 清空buf */
			/* 标准输入流写入缓冲区 */
			
			if (fgets(buf, BUFFSIZE - 1, stdin) == NULL){
				perror("fgets");
				continue;
			}
			/* 缓冲区写入文件 */
			write(fd, buf, strlen(buf));
			/* 忽略大小写比较是否为退出字段 */
			if (strncasecmp(buf, QUIT_STR, strlen(QUIT_STR)) == 0){
				printf("直播间已关闭!\n");
				break;
			}
		}
	}
	else{	//父进程
		while (1){
			bzero(buf, sizeof(buf));
			do{
				/* 读socket */
				ret = read(fd, buf, BUFFSIZE - 1);
			}while (ret < 0 && EINTR == errno);	//读阻塞
			if (ret < 0){
				perror("read");
				exit(-1);
			}
			if (ret == 0){	//对方关闭,(写端不存在)
				printf("< %d > 已退出直播间!\n", getpid());
				break;
			}
			
			/* 若接收到的消息为QUIT_STR,则关闭该socket通信 */
			if (strncasecmp(buf, QUIT_STR, strlen(QUIT_STR)) == 0){
				printf("< %d > 已退出直播间!\n", getpid());
				break;
			}
			if (strlen(buf) != 0)
				printf("< %d > 说: %s", getpid(), buf);
		}
	}
	close(fd);
}
void sigalHandle(int signo)
{
	/* 避免产生僵尸进程 */
	/* 在一个进程终止或者停止时，将SIGCHLD信号发送给其父进程 */
	if (SIGCHLD == signo){
		waitpid(-1, NULL, WNOHANG);
	}
}
