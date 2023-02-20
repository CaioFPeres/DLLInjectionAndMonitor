#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <winsock2.h>
#include "../MinHook/include/MinHook.h"

#pragma comment(lib, "ws2_32.lib")

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
std::string GetLastErrorAsString();

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
    lastSocket = s;
    int x = oWSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);
    int diff = *lpNumberOfBytesRecvd;

    for (int i = 0; i < dwBufferCount; ++i) {
        if (diff > lpBuffers[0].len) {
            recvPipe.write(lpBuffers[i].buf, lpBuffers[i].len);
            recvPipe.flush();
            diff -= lpBuffers[i].len;
        }
        else {
            recvPipe.write(lpBuffers[i].buf, diff);
            recvPipe.flush();
        }
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

            // Get the address of the original function:
            // Uncomment the ones you need to use. They are all working.
            oldSend = (SendFunc) GetProcAddress(GetModuleHandle(L"ws2_32.dll"), "send");
            //oldRecv = (RecvFunc) GetProcAddress(GetModuleHandle(L"ws2_32.dll"), "recv");
            //oldWSARecv = (WSARecvFunc) GetProcAddress(GetModuleHandle(L"ws2_32.dll"), "WSARecv");

            // Create a hook for the function
            MH_CreateHook(oldSend, modSend, (LPVOID*) &oSend);
            //MH_CreateHook(oldRecv, modRecv, (LPVOID*) &oRecv);
            //MH_CreateHook(oldWSARecv, modWSARecv, (LPVOID*)&oWSARecv);

            // Enable the hook
            MH_EnableHook(oldSend);
            //MH_EnableHook(oldRecv);
            //MH_EnableHook(oldWSARecv);

            CreateThread(NULL, NULL, transmitter, NULL, NULL, NULL);
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


DWORD WINAPI transmitter(void*) {

    char buff;

    WaitNamedPipe(L"\\\\.\\pipe\\TransmitterPipe", NMPWAIT_WAIT_FOREVER);
    HANDLE hPipe = CreateFile(L"\\\\.\\pipe\\TransmitterPipe", GENERIC_READ | GENERIC_WRITE | PIPE_WAIT, 0, NULL, OPEN_EXISTING, 0, NULL);

    while (true) {
        DWORD bytesAvailable = 0;
        string msg;

        int status = ReadFile(hPipe, &buff, 1, NULL, NULL);
        msg.push_back(buff);
        PeekNamedPipe(hPipe, NULL, 1, NULL, &bytesAvailable, NULL);

        while (bytesAvailable > 0) {
            int status = ReadFile(hPipe, &buff, 1, NULL, NULL);
            msg.push_back(buff);
            bytesAvailable--;
        }

        // If you dont get the return value, the function just won't work, and i dont know why.
        int num = oSend(lastSocket, msg.c_str(), msg.size(), NULL);

        if (!status) {
            MessageBox(NULL, L"Connection to send pipe lost.", L"Error", MB_OK);
            return 0;
        }
    }
}


/*
* In order to use fstream, i wasn't able to find another way than setting a special delimiter. It also works, but im not using because a byte could end up being
  the same as the delimiter.

DWORD WINAPI transmitter(void*) {
    
    char buff;
    char* byteArr;
    vector<char> vecBuff;

    while (true) {
        while (true) {
            transmitterPipe.read(&buff, 1);
            if (buff == '\n')
                break;
            vecBuff.push_back(buff);
        }

        byteArr = new char[vecBuff.size()];

        for (int i = 0; i < vecBuff.size(); i++) {
            byteArr[i] = vecBuff[i];
        }

        // If you dont get the return value, the function just won't work, and i dont know why.
        int num = oSend(lastSocket, byteArr, vecBuff.size(), NULL);
        delete[] byteArr;
        vecBuff.clear();
    
        if (transmitterPipe.eof()) {
            MessageBox(NULL, L"Connection to send pipe lost.", L"Error", MB_OK);
            return 1;
        }
    }
}
*/

std::string GetLastErrorAsString()
{
    //Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0) {
        return std::string(); //No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;

    //Ask Win32 to give us the string version of that message ID.
    //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    //Copy the error message into a std::string.
    std::string message(messageBuffer, size);

    //Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
}

