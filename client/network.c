#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include "utils/log.h"
#include "network.h"
#include "system.h"

//dag 网络初始化函数
int dag_network_init(void)
{
#ifdef _WIN32
	WSADATA wsaData;
	return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#endif
	return 1;
}

//验证地址
///* pool_address 矿池地址 *peerAddr 对比地址 ** error_message 错误输出信息
static int validate_address(const char* pool_address, struct sockaddr_in *peerAddr, const char** error_message)
{
	char *lasts;
	char buf[0x100] = { 0 };

	// 填写矿池通讯地址（处理网络通信的地址）
	memset(peerAddr, 0, sizeof(struct sockaddr_in));
	peerAddr->sin_family = AF_INET;

	// 解析服务器地址（从符号名转换为IP号）
	strncpy(buf, pool_address, sizeof(buf) - 1);
	char *addressPart = strtok_r(buf, ":", &lasts);
	if(!addressPart) {
		*error_message = "host is not given";
		return 0;
	}
	if(!strcmp(addressPart, "any")) {
		peerAddr->sin_addr.s_addr = htonl(INADDR_ANY);
	} else if(!inet_aton(addressPart, &peerAddr->sin_addr)) {
		struct hostent *host = gethostbyname(addressPart);
		if(!host || !host->h_addr_list[0]) {
			*error_message = "cannot resolve host ";
			return 0;
		}
		// 将服务器解析的IP地址写入地址结构
		memmove(&peerAddr->sin_addr.s_addr, host->h_addr_list[0], 4);
	}

	// 解析端口
	char *portPart = strtok_r(0, ":", &lasts);
	if(!portPart) {
		//mess = "port is not given";
		return 0;
	}
	peerAddr->sin_port = htons(atoi(portPart));

	return 1;
}

//链接矿池地址
int dag_connect_pool(const char* pool_address, const char** error_message)
{
	struct sockaddr_in peeraddr;
	int reuseAddr = 1;
	struct linger lingerOpt = { 1, 0 }; // Linger active, timeout 0

	if(!validate_address(pool_address, &peeraddr, error_message)) {
		return INVALID_SOCKET;
	}

	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sock == INVALID_SOCKET) {
		*error_message = "cannot create a socket";
		return INVALID_SOCKET;
	}
	if(fcntl(sock, F_SETFD, FD_CLOEXEC) == -1) {
		xdag_err("pool : can't set FD_CLOEXEC flag on socket %d, %s\n", sock, strerror(errno));
	}

	//将“LINGER”超时设置为零，关闭监听套接字
	// 即终止程序。
	setsockopt(sock, SOL_SOCKET, SO_LINGER, (char *)&lingerOpt, sizeof(lingerOpt));
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuseAddr, sizeof(int));

	// 现在，连接到一个池
	int res = connect(sock, (struct sockaddr*)&peeraddr, sizeof(struct sockaddr_in));
	if(res) {
		*error_message = "cannot connect to the pool";
		return INVALID_SOCKET;
	}
	return sock;
}

//Dga 关闭链接函数（退出函数）
void dag_connection_close(int socket)
{
	if(socket != INVALID_SOCKET) {
		close(socket);
	}
}
