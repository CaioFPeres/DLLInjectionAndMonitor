#ifndef INJECTOR_H
#define INJECTOR_H

#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <string>

using std::wstring;
using std::cout;

class Injector
{
public:
    Injector(wstring procName);

private:
    int GetProcessByName(wstring name);
    void Inject(int pid);


};

#endif