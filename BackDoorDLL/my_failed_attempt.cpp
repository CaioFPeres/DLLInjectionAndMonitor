/*

    This is just for record


#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <winsock2.h>
#include <windows.h>
#include <tlhelp32.h>

using namespace std;

typedef int (WINAPI* SendFunc)(SOCKET, const char*, int, int);
typedef int (WINAPI* RecvFunc)(SOCKET, char*, int, int);
typedef int (WINAPI* WSARecvFunc)(SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);

SendFunc pSend = NULL;
SendFunc backupSend;
RecvFunc pRecv = NULL;
WSARecvFunc pWSARecv = NULL;

int WINAPI our_send(SOCKET s, const char* buf, int len, int flags);
int WINAPI our_recv(SOCKET s, char* buf, int len, int flags);
int WINAPI our_wsa_recv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

HMODULE hWinsock = GetModuleHandle(L"ws2_32.dll");

fstream sendPipe;
fstream recvPipe;

SOCKET lastSocket = INVALID_SOCKET;

// good example of hooking for x86:
// https://github.com/Zer0Mem0ry/APIHook

// my attempt on writing my own detour function (works, we jump)
void DetourFunction(BYTE* address, PVOID target) {
    DWORD oldProtect;
    VirtualProtect(address, 5, PAGE_EXECUTE_READWRITE, &oldProtect);

    // we should save the original function before writing a jump to it, BUT
    //backupSend = (SendFunc)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    //memcpy(backupSend, &pSend, sizeof(pSend));
    // it just doesn't work

    // Overwrite the first 5 bytes of the function with a jump instruction
    address[0] = 0xE9;
    *(DWORD*)(address + 1) = (DWORD)target - (DWORD)address - 5;

    VirtualProtect(address, 5, oldProtect, &oldProtect);
}

// my attempt on writing my own undetour function (doesn't work)
void UnDetourFunction(BYTE* address, PVOID target) {
    DWORD oldProtect;
    VirtualProtect(address, 5, PAGE_EXECUTE_READWRITE, &oldProtect);

    // Overwrite the first 5 bytes of the function with a jump instruction
    address[0] = 0xE9;
    *(DWORD*)(address + 1) = (BYTE)address + 5;

    VirtualProtect(address, 5, oldProtect, &oldProtect);
}

int GetProcessByName(wstring name)
{
    DWORD pid = -1;

    // Create toolhelp snapshot.
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 process;
    ZeroMemory(&process, sizeof(process));
    process.dwSize = sizeof(process);

    // Walkthrough all processes.
    if (Process32First(snapshot, &process))
    {
        do
        {
            // Compare process.szExeFile based on format of name, i.e., trim file path
            // trim .exe if necessary, etc.
            if (wstring(process.szExeFile) == name)
            {
                pid = process.th32ProcessID;
                break;
            }
        } while (Process32Next(snapshot, &process));
    }

    CloseHandle(snapshot);
    return pid;
}

DWORD procId = GetProcessByName(L"DLLTest.exe");
HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, NULL, procId);


int WINAPI our_send(SOCKET s, const char* buf, int len, int flags)
{
    MessageBox(NULL, L"INTERCEPTED", L"INTERCEPTED", MB_OK);

    // crashing
    backupSend(s, buf, len, flags);

    /*
    lastSocket = s;
    sendPipe.write(buf, len);
    sendPipe.flush();
    /*
    char buf2[len];
    sendPipe.read(buf2, len);
    pSend(s, buf2, sendPipe.gcount(), flags);
    //return len;
    return 0;
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

        pSend = (SendFunc)GetProcAddress(hWinsock, "send");

        DetourFunction((BYTE*)pSend, our_send);

        MessageBox(NULL, L"Didn't crash", L"WORKED", MB_OK);

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
*/