#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <winsock2.h>
#include <windows.h>

/*
int WSARecv(
  __in     SOCKET s,
  __inout  LPWSABUF lpBuffers,
  __in     DWORD dwBufferCount,
  __out    LPDWORD lpNumberOfBytesRecvd,
  __inout  LPDWORD lpFlags,
  __in     LPWSAOVERLAPPED lpOverlapped,
  __in     LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);
*/

using namespace std;

typedef int (WINAPI* SendFunc)(SOCKET, const char*, int, int);
typedef int (WINAPI* RecvFunc)(SOCKET, char*, int, int);
typedef int (WINAPI* WSARecvFunc)(SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);

SendFunc pSend = NULL;
RecvFunc pRecv = NULL;
WSARecvFunc pWSARecv = NULL;

int WINAPI our_send(SOCKET s, const char* buf, int len, int flags);
int WINAPI our_recv(SOCKET s, char* buf, int len, int flags);
int WINAPI our_wsa_recv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

HMODULE hWinsock = GetModuleHandle(L"ws2_32.dll");

fstream sendPipe;
fstream recvPipe;

SOCKET lastSocket = INVALID_SOCKET;

int WINAPI our_send(SOCKET s, const char* buf, int len, int flags)
{
    DWORD tmp;
    lastSocket = s;
    sendPipe.write(buf, len);
    sendPipe.flush();
    /*
    char buf2[len];
    sendPipe.read(buf2, len);
    pSend(s, buf2, sendPipe.gcount(), flags);
    */
    //return len;
    return pSend(s, buf, len, flags);
}

int WINAPI our_recv(SOCKET s, char* buf, int len, int flags)
{
    DWORD tmp;
    lastSocket = s;
    len = pRecv(s, buf, len, flags);
    if (len > 0)
    {
        recvPipe.write(buf, len);
        recvPipe.flush();
    }
    //recvPipe.read(buf, len);
    return len;
}

int WINAPI our_wsa_recv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    int x = pWSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);
    for (int i = 0; i < dwBufferCount; ++i) {
        recvPipe.write(lpBuffers[i].buf, lpBuffers[i].len);
        recvPipe.flush();
    }
    return x;
}

DWORD WINAPI transmitter(void*);


extern "C" __declspec(dllexport) BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            MessageBox(NULL, L"WORKED", L"WORKED", MB_OK);
            // attach to process
            
            DisableThreadLibraryCalls((HMODULE)hinstDLL);
            
            WaitNamedPipe(L"\\\\.\\pipe\\ConquerSendPipe", NMPWAIT_WAIT_FOREVER);
            sendPipe.open(L"\\\\.\\pipe\\ConquerSendPipe");
            
            if (!sendPipe) {
                MessageBox(NULL, L"Failed to connect send pipe.", L"Error", MB_OK);
                return FALSE;
            }

            WaitNamedPipe(L"\\\\.\\pipe\\ConquerRecvPipe", NMPWAIT_WAIT_FOREVER);
            recvPipe.open(L"\\\\.\\pipe\\ConquerRecvPipe");

            if (!recvPipe) {
                MessageBox(NULL, L"Failed to connect recv pipe.", L"Error", MB_OK);
                return FALSE;
            }
            
            //CreateThread(NULL, 0, transmitter, 0, 0, NULL);
            break;
        case DLL_PROCESS_DETACH:
            // detach from process

            break;

        case DLL_THREAD_ATTACH:
            // attach to thread
            break;

        case DLL_THREAD_DETACH:
            // detach from thread
            break;
    }

    return TRUE; // succesful
}

DWORD WINAPI transmitter(void*)
{
    char buf[4096];
    DWORD len;
    while (1) {
        sendPipe.read(buf, 4095);
        len = sendPipe.gcount();
        if (len == 0) {
            MessageBox(NULL, L"Connection to send pipe lost.", L"Error", MB_OK);
            return 1;
        }
        buf[len + 1] = 0;
        if (strcmp(buf, "exit") == 0) { break; }
        pSend(lastSocket, buf, len, 0);
    }
    return 0;
}