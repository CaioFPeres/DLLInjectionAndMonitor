#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <codecvt>
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
typedef int (WSAAPI* WSASendFunc)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef int (WSAAPI* WSARecvFunc)(SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);

SendFunc oSend = NULL;
SendFunc oldSend = NULL;

RecvFunc oRecv = NULL;
RecvFunc oldRecv = NULL;

WSASendFunc oWSASend = NULL;
WSASendFunc oldWSASend = NULL;

WSARecvFunc oWSARecv = NULL;
WSARecvFunc oldWSARecv = NULL;

int WINAPI modSend(SOCKET s, const char* buf, int len, int flags);
int WINAPI modRecv(SOCKET s, char* buf, int len, int flags);
int WINAPI modWSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
int WINAPI modWSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

HANDLE hSendPipe = INVALID_HANDLE_VALUE;
HANDLE hRecvPipe = INVALID_HANDLE_VALUE;

bool usingWSASend = false;
bool usingWSARecv = false;

SOCKET lastSocket = INVALID_SOCKET;


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


DWORD WINAPI SendTransmitter(void*) {

    char buff;

    WaitNamedPipe(L"\\\\.\\pipe\\TransmitterPipe", NMPWAIT_WAIT_FOREVER);
    HANDLE hPipe = CreateFile(L"\\\\.\\pipe\\TransmitterPipe", GENERIC_READ | GENERIC_WRITE, PIPE_WAIT, NULL, OPEN_EXISTING, 0, NULL);

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

        if (usingWSASend) {
            WSABUF WSAbuff = {};
            DWORD lpNumberOfBytesSent = 0;

            WSAbuff.len = msg.size();
            WSAbuff.buf = (CHAR*)msg.c_str();

            // If you dont get the return value, the function just won't work, and i dont know why.
            int num = oWSASend(lastSocket, &WSAbuff, 1, &lpNumberOfBytesSent, NULL, NULL, NULL);
        }
        else {
            // If you dont get the return value, the function just won't work, and i dont know why.
            int num = oSend(lastSocket, msg.c_str(), msg.size(), NULL);
        }


        if (!status) {
            MessageBox(NULL, L"Connection to transmitter pipe lost.", L"Error", MB_OK);
            return 0;
        }
    }
}


int WINAPI modSend(SOCKET s, const char* buf, int len, int flags)
{
    lastSocket = s;
    DWORD bytesWritten;
    if (!usingWSASend) {
        WriteFile(hSendPipe, buf, len, &bytesWritten, NULL);
        FlushFileBuffers(hSendPipe);
    }
    return oSend(s, buf, len, flags);
}

int WINAPI modRecv(SOCKET s, char* buf, int len, int flags)
{
    lastSocket = s;
    len = oRecv(s, buf, len, flags);
    DWORD bytesWritten;
    if (!usingWSARecv && len > 0)
    {
        WriteFile(hRecvPipe, buf, len, &bytesWritten, NULL);
        FlushFileBuffers(hRecvPipe);
    }

    return len;
}

int WINAPI modWSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    lastSocket = s;
    usingWSASend = true;

    int diff = *lpNumberOfBytesSent;
    DWORD bytesWritten;

    for (int i = 0; i < dwBufferCount; ++i) {
        if (diff > lpBuffers[0].len) {
            WriteFile(hSendPipe, lpBuffers[i].buf, lpBuffers[i].len, &bytesWritten, NULL);
            FlushFileBuffers(hSendPipe);
            diff -= lpBuffers[i].len;
        }
        else {
            WriteFile(hSendPipe, lpBuffers[i].buf, diff, &bytesWritten, NULL);
            FlushFileBuffers(hSendPipe);
        }
    }

    return oWSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
}

int WINAPI modWSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    lastSocket = s;
    usingWSARecv = true;
    int x = oWSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);
    
    // This will never trigger. But if you don't verify, it will result in undefined behaviour.
    if (x != 0)
        return x;

    int diff = *lpNumberOfBytesRecvd;
    DWORD bytesWritten;

    for (int i = 0; i < dwBufferCount; ++i) {
        if (diff > lpBuffers[0].len) {
            WriteFile(hRecvPipe, lpBuffers[i].buf, lpBuffers[i].len, &bytesWritten, NULL);
            FlushFileBuffers(hRecvPipe);
            diff -= lpBuffers[i].len;
        }
        else {
            WriteFile(hRecvPipe, lpBuffers[i].buf, diff, &bytesWritten, NULL);
            FlushFileBuffers(hRecvPipe);
        }
    }

    return x;
}


extern "C" __declspec(dllexport) BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{

    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:

            MessageBox(NULL, L"Injected", L"WORKED", MB_OK);
            // attach to process
            
            DisableThreadLibraryCalls((HMODULE)hinstDLL);
            
            WaitNamedPipe(L"\\\\.\\pipe\\ConquerSendPipe", NMPWAIT_WAIT_FOREVER);
            hSendPipe = CreateFile(L"\\\\.\\pipe\\ConquerSendPipe", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, NULL, OPEN_EXISTING, 0, NULL);
            
            if (!hSendPipe) {
                MessageBox(NULL, L"Failed to connect send pipe.", L"Error", MB_OK);
                return FALSE;
            }

            WaitNamedPipe(L"\\\\.\\pipe\\ConquerRecvPipe", NMPWAIT_WAIT_FOREVER);
            hRecvPipe = CreateFile(L"\\\\.\\pipe\\ConquerRecvPipe", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, NULL, OPEN_EXISTING, 0, NULL);

            if (!hRecvPipe) {
                MessageBox(NULL, L"Failed to connect recv pipe.", L"Error", MB_OK);
                return FALSE;
            }

            // Initialize MinHook
            MH_Initialize();

            // Get the address of the original function:
            oldSend = (SendFunc) GetProcAddress(GetModuleHandle(L"ws2_32.dll"), "send");
            oldRecv = (RecvFunc) GetProcAddress(GetModuleHandle(L"ws2_32.dll"), "recv");
            oldWSASend = (WSASendFunc) GetProcAddress(GetModuleHandle(L"ws2_32.dll"), "WSASend");
            oldWSARecv = (WSARecvFunc) GetProcAddress(GetModuleHandle(L"ws2_32.dll"), "WSARecv");

            // Create a hook for the function
            MH_CreateHook(oldSend, modSend, (LPVOID*) &oSend);
            MH_CreateHook(oldRecv, modRecv, (LPVOID*) &oRecv);
            MH_CreateHook(oldWSASend, modWSASend, (LPVOID*)&oWSASend);
            MH_CreateHook(oldWSARecv, modWSARecv, (LPVOID*)&oWSARecv);

            // Enable the hook
            MH_EnableHook(oldSend);
            MH_EnableHook(oldRecv);
            MH_EnableHook(oldWSASend);
            MH_EnableHook(oldWSARecv);

            CreateThread(NULL, NULL, SendTransmitter, NULL, NULL, NULL);

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

