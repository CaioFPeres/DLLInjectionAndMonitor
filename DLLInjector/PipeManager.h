#ifndef PIPEMANAGER_H
#define PIPEMANAGER_H

#include <iostream>
#include <iomanip>
#include <windows.h>
#include <codecvt>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include "Injector.h"


using std::string;
using std::vector;
using std::fstream;

class Window;


class PipeManager {

public:
	PipeManager(Window* win, std::string procNameS);

	HANDLE hSendPipe = INVALID_HANDLE_VALUE;
	HANDLE hRecvPipe = INVALID_HANDLE_VALUE;
	HANDLE TransmitterPipe = INVALID_HANDLE_VALUE;

	HANDLE recvThread;
	HANDLE sendThread;
	HANDLE listenThread;

	Window* winRef;

	wstring GetLastErrorAsString();
	static DWORD Intercept(LPVOID param);
	void Send(const char* byteArr, int len);
	static void Listen(LPVOID param);

private:

	typedef struct tParam {
		PipeManager* tpointer;
		int option;
	};

	wstring sendPipeName, recvPipeName, transmitterPipeName;
	fstream outputFile;

	int ReadBinaryFile(string fileName, char** byteArr);
	int ReadPipeLoop(int option);

	void CreatePipes();
	int ConnectPipes();

};

#endif