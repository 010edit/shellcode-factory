#include "shellcode.h"


extern void ShellcodeFunctionCallExternExample(void);

/* shallcode ���ʾ�� */
SC_EXPORT DWORD ShellcodeFunctionEntryPointExample(LPVOID lpParameter) {

    // �������
    DbgPrint("Thread lpParameter 0x%p", lpParameter);

    // ʹ�� sprintf �� �ַ��� �� �Լ����������� 
    CHAR buf[512] = {0};
    LI_FN(sprintf)(buf, "Hello The thread parameter is 0x%p and The function name is %s", lpParameter,__FUNCTION__);

    //ʹ��ϵͳ API
    LI_FN(MessageBoxA)(HWND(0), buf, xorstr_("Display from shellcode!"), MB_OK | MB_TOPMOST);

    //��.cpp���ú��� ����ͨ�� extern��Ҳ����ͨ���ڹ�ͬͷ�ļ��и�������
    ShellcodeFunctionCallExternExample();

    return 0;
}


/* shallcode VEH ʾ��  */
LONG WINAPI VehExampleHandler(EXCEPTION_POINTERS *pExceptionInfo) { return EXCEPTION_CONTINUE_SEARCH; }
SC_EXPORT DWORD ShellcodeVehExample(LPVOID lpParameter) {
    PVOID veh_hanle = LI_FN(AddVectoredExceptionHandler)(1, &VehExampleHandler);
    if (veh_hanle) {
        LI_FN(RemoveVectoredExceptionHandler)(veh_hanle);
    }
    return 0;
}

