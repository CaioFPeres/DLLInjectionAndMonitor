#include "Window.h"


Window::Window(HINSTANCE hInst, HINSTANCE hInstPrev, std::string procName) {

    this->hInst = hInst;
    this->hInstPrev = hInstPrev;

    _RegisterClass();

    this->manager = new PipeManager(this, procName);

    CreateWindow(
        L"MainWndClass",
        L"Sample",
        WS_OVERLAPPEDWINDOW, 
        CW_USEDEFAULT, 
        CW_USEDEFAULT,
        800, 
        600, 
        (HWND)NULL,
        (HMENU)NULL, 
        hInst, 
        (LPVOID)this);

    ShowWindow(hwndMain, true);
    UpdateWindow(hwndMain);
}

// Register the window class for the main window.
int Window::_RegisterClass() {

    WNDCLASS wc{};

    if (!this->hInstPrev)
    {
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = (WNDPROC)WndProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = this->hInst;
        wc.hIcon = LoadIcon((HINSTANCE)NULL, IDI_APPLICATION);
        wc.hCursor = LoadCursor((HINSTANCE)NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszMenuName = L"MainMenu";
        wc.lpszClassName = L"MainWndClass";

        if (!RegisterClass(&wc))
            return FALSE;
    }
}

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Window* self;
    if (msg == WM_NCCREATE) {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        self = static_cast<Window*>(lpcs->lpCreateParams);
        self->hwndMain = hwnd;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else {
        self = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (self) {
        return self->realWndProc(msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK Window::realWndProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
    RECT rect;

    switch (msg)
    {

        case WM_CREATE:
        {
            CreateControls();
            break;
        }

        case WM_COMMAND:
        {
            if ((HWND)lParam == this->recvButton)
            {
                if (receiving) {
                    receiving = false;
                    SetWindowText(this->recvButton, L"Receive");
                }
                else {
                    receiving = true;
                    SetWindowText(this->recvButton, L"STOP");
                }
            }

            if ((HWND)lParam == this->sendButton) {
                int len = GetWindowTextLength(TextBoxes[1]);
                vector<wchar_t> temp(len + 1, ' ');
                GetWindowText(TextBoxes[1], temp.data(), len + 1); // + 1 for god knows why

                std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
                std::string text = converter.to_bytes(temp.data());

                manager->Send(text.data(), text.size());
            }

            break;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(this->hwndMain, &ps);
            GetClientRect(this->hwndMain, &rect);

            //TextOut(hdc, rect.left, rect.top, L"AAAAA", 5);

            EndPaint(this->hwndMain, &ps);

            return 0;
        }

        case WM_DESTROY:
        {
            PostQuitMessage(0);
            break;
        }

        default:
            return DefWindowProc(this->hwndMain, msg, wParam, lParam);
            break;
    }
    return 0;
}

void Window::CreateControls() {
    RECT rect;
    GetClientRect(this->hwndMain, &rect);

    TextBoxes[0] = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Edit"), NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL, rect.left + 30, rect.top + 30, 300, 300, this->hwndMain, NULL, NULL, NULL);
    TextBoxes[1] = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Edit"), NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL, rect.left + 400, rect.top + 30, 300, 300, this->hwndMain, NULL, NULL, NULL);
    
    StaticControl = CreateWindowEx(NULL, TEXT("Static"), TEXT("Received"), WS_CHILD | WS_VISIBLE, rect.left + 30, rect.top + 10, 80, 15, this->hwndMain, NULL, NULL, NULL);
    StaticControl = CreateWindowEx(NULL, TEXT("Static"), TEXT("Send"), WS_CHILD | WS_VISIBLE, rect.left + 400, rect.top + 10, 50, 15, this->hwndMain, NULL, NULL, NULL);

    recvButton = CreateWindow(L"BUTTON", L"Receive", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, rect.left + 110, 400, 120, 30, this->hwndMain, NULL, NULL, NULL);
    sendButton = CreateWindow(L"BUTTON", L"Send", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, rect.left + 495, 400, 120, 30, this->hwndMain, NULL, NULL, NULL);

}

bool Window::isReceiving() {
    return receiving;
}

void Window::AppendText(int index, string text){
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring wtext = converter.from_bytes(text);

    int len = GetWindowTextLength(TextBoxes[index]);
    vector<wchar_t> temp(len + wtext.size() + 1, ' ');

    GetWindowText(TextBoxes[index], temp.data(), len + 1); // + 1 for god knows why

    wcscat_s(temp.data(), temp.size() + wtext.size() + 1, wtext.data());

    SetWindowText(TextBoxes[index], temp.data());
}

void Window::WindowMessageLoop() {

    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}