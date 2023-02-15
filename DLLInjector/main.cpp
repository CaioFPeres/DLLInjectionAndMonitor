#include <iostream>
#include <iomanip>
#include <windows.h>
#include <codecvt>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <tlhelp32.h>

using namespace std;

static void output_packet(char packet[], int len, fstream* file, int option)
{

    if (option == 1) {
        cout << "\n\nClient --> Server";
        file->write("PClient --> Server", 19);
    }
    else {
        cout << "\n\nServer --> Client";
        file->write("PServer --> Client", 19);
    }

    cout << '\n';
    
    for (int i = 0; i < len; ++i) {        
        cout << hex << setw(2) << right << setfill('0') << (int)(unsigned char)packet[i] << " ";
        file->write((const char*) &packet[i], 1);
    }

    file->flush();
}

DWORD WINAPI server_to_client(LPVOID);
DWORD WINAPI client_to_server(LPVOID);

HANDLE hSendPipe = INVALID_HANDLE_VALUE;
HANDLE hRecvPipe = INVALID_HANDLE_VALUE;
HANDLE TransmitterPipe = INVALID_HANDLE_VALUE;


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


void Inject(int pid) {
    HANDLE processHandle;
    PVOID remoteBuffer;
    char dllPath[] = "C:\\Users\\Caio Peres\\source\\repos\\DLLInjector\\Debug\\BackDoorDLL.dll";
    unsigned int dllLen = sizeof(dllPath) + 1;

    cout << "Injecting DLL to PID: " + to_string(pid) << '\n';
    processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    remoteBuffer = VirtualAllocEx(processHandle, NULL, dllLen, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

    if (remoteBuffer == NULL) {
        cout << "Error\n";
        return;
    }

    if (WriteProcessMemory(processHandle, remoteBuffer, dllPath, dllLen, NULL) == false) {
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


int ReadBinaryFile(string fileName, char** byteArr){
    char sc;
    vector<char> byteVector;
    fstream packetFile(fileName, fstream::in | fstream::binary);

    while (packetFile.get(sc)) {
        byteVector.push_back(sc);
    }

    *byteArr = new char[byteVector.size()]; // + 1

    for (int i = 0; i < byteVector.size(); i++) {
        (*byteArr)[i] = byteVector[i];
    }

    //(*byteArr)[byteVector.size()] = '\n';

    return byteVector.size(); // + 1
}


int main(int argc, char* argv[])
{

    if (argc != 2) {
        cout << "Usage: DLLInjector.exe processName.exe" << endl;
        return 1;
    }

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring procName = converter.from_bytes(argv[1]);

    int pid = GetProcessByName(procName);

    if (pid < 0) {
        cout << "Could not find process name\n";
        return 0;
    }
    
    // create a named pipe for communicating with the remote process
    wstring sendPipeName, recvPipeName, transmitterPipeName;
    sendPipeName = L"\\\\.\\pipe\\ConquerSendPipe";
    recvPipeName = L"\\\\.\\pipe\\ConquerRecvPipe";
    transmitterPipeName = L"\\\\.\\pipe\\TransmitterPipe";

    wcout << "send pipe name: " << sendPipeName << endl;
    wcout << "recv pipe name: " << recvPipeName << endl;
    wcout << "transmitter pipe name: " << transmitterPipeName << endl;
    wcout << "Creating pipes\n";

    hSendPipe = CreateNamedPipe(sendPipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, 4096, 4096, 0, NULL); //| FILE_FLAG_OVERLAPPED, PIPE_UNLIMITED_INSTANCES
    hRecvPipe = CreateNamedPipe(recvPipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    TransmitterPipe = CreateNamedPipe(transmitterPipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);

    Inject(pid);

    WaitNamedPipe(sendPipeName.c_str(), NMPWAIT_WAIT_FOREVER);
    WaitNamedPipe(recvPipeName.c_str(), NMPWAIT_WAIT_FOREVER);
    WaitNamedPipe(transmitterPipeName.c_str(), NMPWAIT_WAIT_FOREVER);

    if (hSendPipe == INVALID_HANDLE_VALUE) {
        cout << "failed to create send pipe." << endl;
        return 1;
    }
    
    cout << "Connecting send pipe... ";
    if (!ConnectNamedPipe(hSendPipe, NULL)) {
        cout << "failed to connect client to send pipe." << endl;
        return 1;
    }
    
    cout << "done." << endl;
    cout << "Connecting recv pipe... ";
    
    if (hRecvPipe == INVALID_HANDLE_VALUE) {
        cout << "failed to create recv pipe." << endl;
        return 1;
    }

    // Taken from MS docs:
    /* 
        If a client connects before the function is called, the function returns zero and GetLastError returns ERROR_PIPE_CONNECTED.
        This can happen if a client connects in the interval between the call to CreateNamedPipe and the call to ConnectNamedPipe.
        In this situation, there is a good connection between client and server, even though the function returns zero. 
    */
    
    if (!ConnectNamedPipe(hRecvPipe, NULL)) {
        if (GetLastError() != ERROR_PIPE_CONNECTED) {
            cout << "failed to connect client to recv pipe." << endl;
            return 1;
        }
    }

    cout << "done." << endl;
    cout << "Connecting transmitter pipe... ";

    if (TransmitterPipe == INVALID_HANDLE_VALUE) {
        cout << "failed to create TransmitterPipe." << endl;
        return 1;
    }

    // Taken from MS docs:
    /*
        If a client connects before the function is called, the function returns zero and GetLastError returns ERROR_PIPE_CONNECTED.
        This can happen if a client connects in the interval between the call to CreateNamedPipe and the call to ConnectNamedPipe.
        In this situation, there is a good connection between client and server, even though the function returns zero.
    */

    if (!ConnectNamedPipe(TransmitterPipe, NULL)) {
        if (GetLastError() != ERROR_PIPE_CONNECTED) {
            cout << "failed to connect client to TransmitterPipe." << endl;
            return 1;
        }
    }

    cout << "done." << endl;
    cout << "ready for data." << endl;

    fstream* outputFile = new fstream();
    outputFile->open("packets.pak", fstream::out | fstream::binary);

    HANDLE thread1 = CreateThread(NULL, NULL, server_to_client, (LPVOID) outputFile, NULL, NULL);
    HANDLE thread2 = CreateThread(NULL, NULL, client_to_server, (LPVOID) outputFile, NULL, NULL);

    while (true) {
        
        int a;
        cin >> a;
        cin.clear();
        cin.ignore(INT_MAX, '\n');


        char* byteArr;
        int len = ReadBinaryFile("sendPacket.pak", &byteArr);

        /*
        for (int i = 0; i < len; ++i) {
            cout << hex << setw(2) << right << setfill('0') << (int)(unsigned char) byteArr[i] << " ";
        }
        */

        WriteFile(TransmitterPipe, byteArr, len, NULL, NULL);
        free(byteArr);
    }

    //while (WaitForSingleObject(thread1, 1000) != WAIT_OBJECT_0) {}
    //while (WaitForSingleObject(thread2, 1000) != WAIT_OBJECT_0) {}

    DisconnectNamedPipe(hSendPipe);
    DisconnectNamedPipe(hRecvPipe);
    DisconnectNamedPipe(TransmitterPipe);

    CloseHandle(hSendPipe);
    CloseHandle(hRecvPipe);
    CloseHandle(TransmitterPipe);

    
    return 0;

}

DWORD WINAPI server_to_client(LPVOID param)
{
    fstream* file = (fstream*)param;

    char buf[4096];
    int len = 0;
    while (1) {
        if (!ReadFile(hRecvPipe, buf, 4096, (LPDWORD) &len, NULL)) { break;}
        output_packet(buf, len, file, 2);
    }

    return 0;
}

DWORD WINAPI client_to_server(LPVOID param)
{
    fstream* file = (fstream*)param;

    char buf[4096];
    int len = 0;
    while (1) {
        if (!ReadFile(hSendPipe, buf, 4096, (LPDWORD) &len, NULL)) { break;}
        output_packet(buf, len, file, 1);
    }
    return 0;
}