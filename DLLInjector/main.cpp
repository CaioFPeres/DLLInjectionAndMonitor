#include <iostream>
#include <windows.h>
#include <string>
#include "Window.h"

using std::cout;

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	Window mainWin(hInst, hInstPrev, "DLLTest.exe");
	mainWin.WindowMessageLoop();

	return 0;
}