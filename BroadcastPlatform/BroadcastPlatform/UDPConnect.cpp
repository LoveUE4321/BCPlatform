#include "UDPConnect.h"

#include <iostream>
//添加动态库的lib
#pragma comment(lib, "ws2_32.lib")

#define _WINSOCK_DEPRECATED_NO_WARNINGS

UDPConnect::UDPConnect()
{

}

bool UDPConnect::InitUDP()
{
	WSADATA  wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		//cout << "WSAStartup error:" << GetLastError() << endl;
		return false;
	}

	return true;
}

bool UDPConnect::ConnectUDP()
{
	m_Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_Socket == INVALID_SOCKET)
	{
		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;
		return false;
	}

	// 远端地址
	const char* ip = "192.168.16.1";
	int	port = 8080;
	m_RemoteAdd.sin_family = AF_INET;
	m_RemoteAdd.sin_port = htons(port);
	m_RemoteAddLen = sizeof(m_RemoteAdd);
	inet_pton(AF_INET, ip, &m_RemoteAdd.sin_addr);

	return true;
}

void UDPConnect::SendMsg() 
{
	char sendBuf[1024] = "Nice to meet you!";

	int sendLen = sendto(m_Socket, sendBuf, strlen(sendBuf), 0, (sockaddr*)&m_RemoteAdd, m_RemoteAddLen);
	if (sendLen > 0) {
		//std::printf("发送到远程端连接, 其ip: %s, port: %d\n", inet_ntoa(m_RemoteAdd.sin_addr), ntohs(m_RemoteAdd.sin_port));
		//cout << "发送到远程端的信息： " << sendBuf << endl;
	}
	
}

void UDPConnect::RecvMsg()
{
	char recvBuf[1024] = { 0 };
	int recvLen = recvfrom(m_Socket, recvBuf, 1024, 0, NULL, NULL);
	if (recvLen > 0) {
		//std::printf("接收到一个连接, 其ip: %s, port: %d\n", inet_ntoa(m_RemoteAdd.sin_addr), ntohs(m_RemoteAdd.sin_port));
		//cout << "接收到一个信息： " << recvBuf << endl;
	}
}

void UDPConnect::UnInitUDP()
{
	closesocket(m_Socket);
	WSACleanup();
}