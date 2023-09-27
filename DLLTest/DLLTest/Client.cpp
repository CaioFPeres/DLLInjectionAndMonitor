#include "Client.h"

Client::Client() {

    client_socket = 0;
    connection_status = 0;
    saddr = { 0 };

    //initializing WSA windows
    if (WSAStartup(MAKEWORD(2, 0), &wsa) != 0) {
        wprintf(L"Erro: %ld\n", WSAGetLastError());
        WSACleanup();
        return;
    }
    //////////////////////

    saddr.sin_family = AF_INET;
    InetPton(AF_INET, L"127.0.0.1", &saddr.sin_addr.s_addr);
    saddr.sin_port = htons(2000);
}

void Client::Connect() {

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    connection_status = connect(client_socket, (struct sockaddr*)&saddr, sizeof(saddr));

    //verifica se tem erro de conexão
    if (connection_status == SOCKET_ERROR) {
        wprintf(L"Erro: %ld\n", WSAGetLastError());
        connection_status = closesocket(client_socket);
        WSACleanup();
        return;
    }
}


void Client::Send(string msg) {
    send(client_socket, msg.c_str(), msg.length() + 1, 0);
}