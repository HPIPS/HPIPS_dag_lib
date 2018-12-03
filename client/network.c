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

//dag �����ʼ������
int dag_network_init(void)
{
#ifdef _WIN32
	WSADATA wsaData;
	return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#endif
	return 1;
}

//��֤��ַ
///* pool_address ��ص�ַ *peerAddr �Աȵ�ַ ** error_message ���������Ϣ
static int validate_address(const char* pool_address, struct sockaddr_in *peerAddr, const char** error_message)
{
	char *lasts;
	char buf[0x100] = { 0 };

	// ��д���ͨѶ��ַ����������ͨ�ŵĵ�ַ��
	memset(peerAddr, 0, sizeof(struct sockaddr_in));
	peerAddr->sin_family = AF_INET;

	// ������������ַ���ӷ�����ת��ΪIP�ţ�
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
		// ��������������IP��ַд���ַ�ṹ
		memmove(&peerAddr->sin_addr.s_addr, host->h_addr_list[0], 4);
	}

	// �����˿�
	char *portPart = strtok_r(0, ":", &lasts);
	if(!portPart) {
		//mess = "port is not given";
		return 0;
	}
	peerAddr->sin_port = htons(atoi(portPart));

	return 1;
}

//���ӿ�ص�ַ
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

	//����LINGER����ʱ����Ϊ�㣬�رռ����׽���
	// ����ֹ����
	setsockopt(sock, SOL_SOCKET, SO_LINGER, (char *)&lingerOpt, sizeof(lingerOpt));
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuseAddr, sizeof(int));

	// ���ڣ����ӵ�һ����
	int res = connect(sock, (struct sockaddr*)&peeraddr, sizeof(struct sockaddr_in));
	if(res) {
		*error_message = "cannot connect to the pool";
		return INVALID_SOCKET;
	}
	return sock;
}

//Dga �ر����Ӻ������˳�������
void dag_connection_close(int socket)
{
	if(socket != INVALID_SOCKET) {
		close(socket);
	}
}
