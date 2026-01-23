#pragma once

#include <winsock2.h>
#include <WS2tcpip.h>

class UDPConnect
{
public:
	UDPConnect();

public:
	bool InitUDP();
	bool ConnectUDP();
	void SendMsg();
	void RecvMsg();

	void UnInitUDP();
private:
	SOCKET m_Socket;
	SOCKADDR_IN m_RemoteAdd;
	int m_RemoteAddLen;
};

