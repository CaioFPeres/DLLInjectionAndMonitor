#include <iostream>
#include <WinSock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")


using std::string;


class Client
{

private:
	WSADATA wsa;
	sockaddr_in saddr;
	SOCKET client_socket;
	int connection_status;

public:
	Client();
	void Connect();
	void Send(string msg);

};
