#include <iostream>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <vector>
#pragma comment(lib, "ws2_32.lib")

using std::string;
using std::cout;

class Server
{

private:

	WSADATA wsa;

	sockaddr_in saddr;
	SOCKET client_socket;
	SOCKET server_socket;

public:
	Server();
	void Listen();
};

