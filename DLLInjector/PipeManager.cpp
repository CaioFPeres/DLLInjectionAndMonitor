#include "PipeManager.h"
#include "Window.h"


PipeManager::PipeManager(Window* win, std::string procNameS) {

    this->winRef = win;

    outputFile.open("packets.pak", fstream::out | fstream::binary);

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring procName = converter.from_bytes(procNameS);

    CreatePipes();
    Injector inj(procName);
    ConnectPipes();

    /*
    char* byteArr;
    int len = ReadBinaryFile("sendPacket.pak", &byteArr);
    */

    listenThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)PipeManager::Listen, this, NULL, NULL);
}

int PipeManager::ConnectPipes() {

    WaitNamedPipe(sendPipeName.c_str(), NMPWAIT_WAIT_FOREVER);
    WaitNamedPipe(recvPipeName.c_str(), NMPWAIT_WAIT_FOREVER);
    WaitNamedPipe(transmitterPipeName.c_str(), NMPWAIT_WAIT_FOREVER);

    if (hSendPipe == INVALID_HANDLE_VALUE) {
        OutputDebugString(L"failed to create send pipe");
        return 0;
    }

    cout << "Connecting send pipe... ";
    if (!ConnectNamedPipe(hSendPipe, NULL)) {
        OutputDebugString(L"failed to connect client to send pipe");
        return 0;
    }

    cout << "done." << '\n';
    cout << "Connecting recv pipe... ";

    if (hRecvPipe == INVALID_HANDLE_VALUE) {
        OutputDebugString(L"failed to create recv pipe");
        return 0;
    }

    // Taken from MS docs:
    /*
    If a client connects before the function is called, the function returns zero and GetLastError returns ERROR_PIPE_CONNECTED.
    This can happen if a client connects in the interval between the call to CreateNamedPipe and the call to ConnectNamedPipe.
    In this situation, there is a good connection between client and server, even though the function returns zero.
    */

    if (!ConnectNamedPipe(hRecvPipe, NULL)) {
        if (GetLastError() != ERROR_PIPE_CONNECTED) {
            OutputDebugString(L"failed to connect client to recv pipe");
            return 0;
        }
    }

    cout << "done." << '\n';
    cout << "Connecting transmitter pipe... ";

    if (TransmitterPipe == INVALID_HANDLE_VALUE) {
        OutputDebugString(L"failed to create TransmitterPipe");
        return 0;
    }

    // Taken from MS docs:
    /*
        If a client connects before the function is called, the function returns zero and GetLastError returns ERROR_PIPE_CONNECTED.
        This can happen if a client connects in the interval between the call to CreateNamedPipe and the call to ConnectNamedPipe.
        In this situation, there is a good connection between client and server, even though the function returns zero.
    */

    if (!ConnectNamedPipe(TransmitterPipe, NULL)) {
        if (GetLastError() != ERROR_PIPE_CONNECTED) {
            OutputDebugString(L"failed to connect client to TransmitterPipe");
            return 0;
        }
    }
}

void PipeManager::CreatePipes() {

    // create a named pipe for communicating with the remote process
    sendPipeName = L"\\\\.\\pipe\\ConquerSendPipe";
    recvPipeName = L"\\\\.\\pipe\\ConquerRecvPipe";
    transmitterPipeName = L"\\\\.\\pipe\\TransmitterPipe";

    // BYTE MODE
    hSendPipe = CreateNamedPipe(sendPipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    hRecvPipe = CreateNamedPipe(recvPipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    TransmitterPipe = CreateNamedPipe(transmitterPipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
}

int PipeManager::ReadBinaryFile(string fileName, char** byteArr) {
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


void PipeManager::Listen(LPVOID param) {

    PipeManager* manager = (PipeManager*) param;

    tParam tPSC = { manager, 1 };
    tParam tPCS = { manager, 2 };

    manager->recvThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE) PipeManager::Intercept, &tPSC, NULL, NULL);
    manager->sendThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE) PipeManager::Intercept, &tPCS, NULL, NULL);

    WaitForSingleObject(manager->recvThread, INFINITE);
    WaitForSingleObject(manager->sendThread, INFINITE);

    DisconnectNamedPipe(manager->hSendPipe);
    DisconnectNamedPipe(manager->hRecvPipe);
    DisconnectNamedPipe(manager->TransmitterPipe);

    CloseHandle(manager->hSendPipe);
    CloseHandle(manager->hRecvPipe);
    CloseHandle(manager->TransmitterPipe);
}

void PipeManager::Send(const unsigned char* byteArr, int len) {
    DWORD bytesWritten;
    WriteFile(TransmitterPipe, byteArr, len, &bytesWritten, NULL);
}

int PipeManager::ReadPipeLoop(int option)
{
    while (true) {
        char buff = 0;
        DWORD bytesAvailable = 0;
        string msg;
        string msgHeader;
        HANDLE pipe;

        if( option == 1)
            pipe = hSendPipe;
        else
            pipe = hRecvPipe;

        int status = ReadFile(pipe, &buff, 1, NULL, NULL);
        msg.push_back(buff);

        PeekNamedPipe(pipe, NULL, 1, NULL, &bytesAvailable, NULL);

        while (bytesAvailable > 0) {
            int status = ReadFile(pipe, &buff, 1, NULL, NULL);
            msg.push_back(buff);
            bytesAvailable--;
        }

        if (!status)
            return 0;

        if (option == 1) {
            msgHeader = "\r\n\r\nClient --> Server";
            outputFile.write("PClient --> Server", 18);
        }
        else {
            msgHeader = "\r\n\r\nServer --> Client";
            outputFile.write("PServer --> Client", 18);
        }

        msgHeader += " [" + std::to_string(msg.length()) + "]\r\n";

        std::stringstream ssPacket;
        
        for (int i = 0; i < msg.size(); ++i) {
            // This line should print the correct hexadecimal representation. It was previously causing an undefined behaviour: modifying the contents read by ReadBinaryFile.
            // It seems that its not anymore, but beware.
            ssPacket << std::hex << std::setw(2) << std::right << std::setfill('0') << (int)(unsigned char)msg[i] << " ";
        }

        
        if(winRef->isReceiving())
            this->winRef->AppendText(0, msgHeader + ssPacket.str() + "\r\n\r\n");
        
        outputFile.write(msg.c_str(), msg.size());

        outputFile.flush();
    }
}

DWORD PipeManager::Intercept(LPVOID param) {
    tParam* tP = (tParam*)param;

    if (!tP->tpointer->ReadPipeLoop(tP->option))
        if(tP->option == 1)
            MessageBox(NULL, L"Connection to recv pipe lost.", L"Error", MB_OK);
        else
            MessageBox(NULL, L"Connection to send pipe lost.", L"Error", MB_OK);
    return 0;
}

wstring PipeManager::GetLastErrorAsString()
{
    //Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0) {
        return std::wstring(); //No error message has been recorded
    }

    LPWSTR messageBuffer = nullptr;

    //Ask Win32 to give us the string version of that message ID.
    //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, NULL);

    //Copy the error message into a std::string.
    std::wstring message(messageBuffer, size);

    //Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
}