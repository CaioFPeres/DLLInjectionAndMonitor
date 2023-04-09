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

string GetLastErrorAsString();
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
    char dllPath[] = "BackDoorDLL.dll";
    char fullFilename[MAX_PATH];

    // needs to be the full path, since the application will be running in another folder than this application
    GetFullPathNameA(dllPath, MAX_PATH, fullFilename, nullptr);
    int dllLen = strlen(fullFilename);

    cout << "Injecting DLL to PID: " + to_string(pid) << '\n';
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

int ReadBinaryFile(string fileName, char** byteArr) {
    char sc;
    vector<char> byteVector;
    fstream packetFile(fileName, fstream::in | fstream::binary);

    while (packetFile.get(sc)) {
        byteVector.push_back(sc);
    }
    
    *byteArr = new char[byteVector.size()];

    for (int i = 0; i < byteVector.size(); i++) {
        (*byteArr)[i] = byteVector[i];
    }

    packetFile.close();
    return byteVector.size();
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

    // BYTE MODE
    hSendPipe = CreateNamedPipe(sendPipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    hRecvPipe = CreateNamedPipe(recvPipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    TransmitterPipe = CreateNamedPipe(transmitterPipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);

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

    char* byteArr;
    int len = ReadBinaryFile("sendPacket.pak", &byteArr);
    
    while (true) {

        int a = 0;
        cin >> a;
        cin.clear();
        cin.ignore(INT_MAX, '\n');

        DWORD bytesWritten;
        WriteFile(TransmitterPipe, byteArr, len, &bytesWritten, NULL);
        cout << "Sent: " << bytesWritten << " bytes" << '\n';
    }

    free(byteArr);

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

int ReadPipeLoop(fstream* file, HANDLE pipe, int option)
{
    while (true) {
        char buff = 0;
        DWORD bytesAvailable = 0;
        string msg;

        int status = ReadFile(pipe, &buff, 1, NULL, NULL);
        msg.push_back(buff);

        PeekNamedPipe(pipe, NULL, 1, NULL, &bytesAvailable, NULL);

        while (bytesAvailable > 0) {
            int status = ReadFile(pipe, &buff, 1, NULL, NULL);
            msg.push_back(buff);
            bytesAvailable--;
        }

        if (!status) {
            return 0;
        }


        if (option == 1) {
            cout.write("\n\nClient --> Server", 19);
            file->write("PClient --> Server", 19);
        }
        else {
            cout.write("\n\nServer --> Client", 19);
            file->write("PServer --> Client", 19);
        }

        cout << " [" + to_string(msg.length()) + "]";
        cout << '\n' << msg << '\n';
        file->write(msg.c_str(), msg.size());

        /*
        for (int i = 0; i < msg.size(); ++i) {
            // This line should print the correct hexadecimal representation. It is currently causing an undefined behaviour: it is modifying the contents read by ReadBinaryFile.
            cout << hex << setw(2) << right << setfill('0') << (int)(unsigned char)packet[i] << " ";
        }
        */
        
        file->flush();
    }
}


DWORD WINAPI client_to_server(LPVOID param)
{
    fstream* file = (fstream*)param;
    if(!ReadPipeLoop(file, hSendPipe, 1))
        MessageBox(NULL, L"Connection to send pipe lost.", L"Error", MB_OK);
    return 0;
}

DWORD WINAPI server_to_client(LPVOID param)
{
    fstream* file = (fstream*)param;
    if(!ReadPipeLoop(file, hRecvPipe, 2))
        MessageBox(NULL, L"Connection to recv pipe lost.", L"Error", MB_OK);
    return 0;
}

string GetLastErrorAsString()
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