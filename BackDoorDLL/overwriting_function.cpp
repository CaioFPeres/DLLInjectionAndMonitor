/*

#include <Windows.h>
#include <iostream>

/* 
   This example uses a basic inline hooking technique, which involves replacing the first few bytes of the recv() function with a jump to the hook function hook_recv(). 
   When the DLL is loaded into the process, the hook function is installed, and every time recv() is called, the hook function is executed instead. 
   The hook function retrieves the data that was received and outputs it to the console.
*/

/*

typedef int(WINAPI* recv_t)(SOCKET s, char* buf, int len, int flags);
recv_t original_recv = (recv_t)GetProcAddress(GetModuleHandle(L"ws2_32.dll"), "recv");

int WINAPI hook_recv(SOCKET s, char* buf, int len, int flags)
{
    int ret = original_recv(s, buf, len, flags);
    if (ret > 0) {
        std::cout << "Received data: ";
        for (int i = 0; i < ret; i++) {
            std::cout << buf[i];
        }
        std::cout << std::endl;
    }
    return ret;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        if (original_recv == NULL) {
            return FALSE;
        }
        // Replace the address of the recv() function with the address of the hook_recv() function
        DWORD oldProtect;
        VirtualProtect(original_recv, sizeof(hook_recv), PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(original_recv, &hook_recv, sizeof(hook_recv));
        VirtualProtect(original_recv, sizeof(hook_recv), oldProtect, &oldProtect);
        break;
    case DLL_PROCESS_DETACH:
        // Restore the original recv() function
        DWORD oldProtect;
        VirtualProtect(original_recv, sizeof(hook_recv), PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(original_recv, &original_recv, sizeof(hook_recv));
        VirtualProtect(original_recv, sizeof(hook_recv), oldProtect, &oldProtect);
        break;
    }
    return TRUE;
}
*/