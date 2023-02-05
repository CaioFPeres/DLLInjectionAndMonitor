#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <winsock2.h>
#include "../MinHook/include/MinHook.h"


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

To do:

int WSAAPI WSASend(
  [in]  SOCKET                             s,
  [in]  LPWSABUF                           lpBuffers,
  [in]  DWORD                              dwBufferCount,
  [out] LPDWORD                            lpNumberOfBytesSent,
  [in]  DWORD                              dwFlags,
  [in]  LPWSAOVERLAPPED                    lpOverlapped,
  [in]  LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);

*/

using namespace std;

typedef int (WINAPI* SendFunc)(SOCKET, const char*, int, int);
typedef int (WINAPI* RecvFunc)(SOCKET, char*, int, int);
typedef int (WINAPI* WSARecvFunc)(SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);

SendFunc oSend = NULL;
SendFunc oldSend = NULL;

RecvFunc oRecv = NULL;
RecvFunc oldRecv = NULL;

WSARecvFunc oWSARecv = NULL;
WSARecvFunc oldWSARecv = NULL;

int WINAPI modSend(SOCKET s, const char* buf, int len, int flags);
int WINAPI modRecv(SOCKET s, char* buf, int len, int flags);
int WINAPI modWSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

fstream sendPipe;
fstream recvPipe;

SOCKET lastSocket = INVALID_SOCKET;


int WINAPI modSend(SOCKET s, const char* buf, int len, int flags)
{
    lastSocket = s;
    sendPipe.write(buf, len);
    sendPipe.flush();
    return oSend(s, buf, len, flags);
}

int WINAPI modRecv(SOCKET s, char* buf, int len, int flags)
{
    lastSocket = s;
    len = oRecv(s, buf, len, flags);
    if (len > 0)
    {
        recvPipe.write(buf, len);
        recvPipe.flush();
    }
    return len;
}

int WINAPI modWSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    int x = oWSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);
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

            MessageBox(NULL, L"Injected", L"WORKED", MB_OK);
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

            // Initialize MinHook
            MH_Initialize();

            // Get the address of the original function
            oldSend = (SendFunc) GetProcAddress(GetModuleHandle(L"ws2_32.dll"), "send");
            oldRecv = (RecvFunc) GetProcAddress(GetModuleHandle(L"ws2_32.dll"), "recv");

            // Create a hook for the function
            MH_CreateHook(oldSend, modSend, (LPVOID*) &oSend);
            MH_CreateHook(oldRecv, modRecv, (LPVOID*) &oRecv);

            // Enable the hook
            MH_EnableHook(oldSend);
            MH_EnableHook(oldRecv);

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
        oSend(lastSocket, buf, len, 0);
    }
    return 0;
}