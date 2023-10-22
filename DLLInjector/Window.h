#ifndef WINDOW_H
#define WINDOW_H

#include <iostream>
#include <windows.h>
#include <string>
#include "PipeManager.h"


class Window {

public:

    HWND hwndMain;

    HWND TextBoxes[2];
    wstring TextBoxesContent[2];

    HWND recvLabel;
    HWND sendLabel;
    HWND sendDecoded;

    HWND recvButton; 
    HWND sendButton;

    HINSTANCE hInst;
    HINSTANCE hInstPrev;

    Window(HINSTANCE hInst, HINSTANCE hInstPrev, std::string procName);
    void AppendText(int index, string text);
    vector<unsigned char> HexStringToByte(string hexString);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT CALLBACK realWndProc(UINT msg, WPARAM wParam, LPARAM lParam);
    void WindowMessageLoop();
    bool isReceiving();

private:

    typedef struct tParam {
        Window* tpointer;
        std::string procName;
    };

    HANDLE managerThread;
    PipeManager* manager;
    bool receiving = false;

    string GetControlString(HWND control);
    wstring GetControlWString(HWND control);
    void CreateControls();
    int _RegisterClass();

};

#endif