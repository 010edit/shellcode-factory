#include "shellcode.h"
#include "dwm-capture.h"
SC_EXPORT 
DWORD ShellCodeEntryPoint(LPVOID lpParameter) {
    CHAR buf[256] = {0};
    LI_FN(sprintf)
    (buf, xorstr_("����%s �̲߳���0x%p"), __FUNCDNAME__, lpParameter);
    LI_FN(MessageBoxA)(HWND(0), buf, xorstr_("����shellcode��չʾ"), MB_OK);
    return 0;
}