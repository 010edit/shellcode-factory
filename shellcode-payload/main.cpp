
#include "lazy_importer.hpp"
#ifndef _M_IX86
#include "xorstr.hpp"
#else
#define xorstr_(str) (str)
#endif // 

#include <Windows.h>
#include <cstdio>
#define SC_EXPORT extern "C" _declspec(dllexport)

SC_EXPORT 
DWORD ShellCodeEntryPoint(LPVOID lpParameter) {
    CHAR buf[256] = {0};
    LI_FN(sprintf)(buf, xorstr_( "����%s �̲߳���0x%p"), __FUNCDNAME__, lpParameter);
    LI_FN(MessageBoxA)(HWND(0), buf, xorstr_("����shellcode��չʾ"), MB_OK);
    return 0;
}

SC_EXPORT 
DWORD ShellCodeEntryPoint2(LPVOID lpParameter) {
    CHAR buf[256] = {0};
    LI_FN(sprintf)(buf, xorstr_("����%s �̲߳���0x%p"), __FUNCDNAME__, lpParameter);
    LI_FN(MessageBoxA)(HWND(0), buf, xorstr_("����shellcode��չʾ"), MB_OK);
    return 0;
}

SC_EXPORT
DWORD ShellCodeEntryPoint3(LPVOID lpParameter) {
    CHAR buf[256] = {0};
    LI_FN(sprintf)(buf,xorstr_( "����%s �̲߳���0x%p"), __FUNCDNAME__, lpParameter);
    LI_FN(MessageBoxA)(HWND(0), buf, xorstr_("����shellcode��չʾ"), MB_OK);
    return 0;
}