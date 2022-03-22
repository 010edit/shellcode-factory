#include "dwm-capture.h"
#include "shellcode.h"
#include <dxgi.h>
#include <d3d11.h>
#include <wrl.h>
#define _GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)                                                     \
    GUID name = {l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8};

// ����ֱ�Ӱ� Nt�����Ķ����������ȡ���ɣ����� lazy_importer ��ȡ����������
PVOID NTAPI RtlAddVectoredExceptionHandler(IN ULONG FirstHandler, IN PVECTORED_EXCEPTION_HANDLER VectoredHandler);
ULONG NTAPI RtlRemoveVectoredExceptionHandler(IN PVOID VectoredHandlerHandle);

LONG WINAPI VehHandler(EXCEPTION_POINTERS *pExceptionInfo);
void        TakeDxgiCapture(IUnknown *pDXGISwapChain);
__int64 __fastcall HookFunCallBack(IUnknown *pDXGISwapChain, __int64 a2, __int64 a3, __int64 a4, __int64 a5, __int64 a6,
                                   __int64 a7, __int64 a8);

//���Խ��Ż����������ȫ�ֱ������ʹ�� volatile ����

//ʹ�ù����ľ�̬�͵�������
SC_EXPORT_DATA(volatile __int64, hook_offsets[4]) // hook����ƫ�ƣ���ע������ע��shellcodeǰ����
SC_EXPORT_DATA(volatile __int64, CaptureBitmapPointer) // �������� Ŀ������ע����ȷ��ƫ��λ��
SC_EXPORT_DATA(volatile unsigned int, CaptureWidth)
SC_EXPORT_DATA(volatile unsigned int, CaptureHeight)

// ʹ��ȫ�ֺ;�̬����
volatile unsigned long hook_fun_execute      = 0; // hook�����Ƿ�ִ��
volatile unsigned long  hook_fun_done         = 0; //��ͼ�����Ƿ�ִ��
volatile DWORD          hook_fun_memory_proct = 0; //��hook�����ڴ�����
volatile DWORD64         hook_fun_address      = 0; // hook������ַ

//ʹ����Ƕ���� �������ֻ�ڱ�cpp�����ã���Ҫд��.h���� д��ÿ��cpp���ͷ����
extern "C" {
#pragma function(memset)
void *__cdecl memset(void *dest, int value, size_t num) {
    __stosb(static_cast<unsigned char *>(dest), static_cast<unsigned char>(value), num);
    return dest;
}
#pragma function(memcpy)
void *__cdecl memcpy(void *dest, const void *src, size_t num) {
    __movsb(static_cast<unsigned char *>(dest), static_cast<const unsigned char *>(src), num);
    return dest;
}
}

SC_EXPORT DWORD DwmCaptureScreen(LPVOID lpParameter) {

    DbgPrint("Dwm Capture Screen Thread Entered!");

#ifdef _DEBUG
    for (size_t idx = 0; idx < ArrNum(hook_offsets); idx++) {
        if (hook_offsets[idx] == 0) {
            DbgPrint("Null offset");
            return -1;
        } else {
            HMODULE hDxgi = LI_FN(GetModuleHandleA)("dxgi.dll");
            DbgPrint("Hook Offset 0x%p address 0x%p", hook_offsets[idx], hook_offsets[idx] + (DWORD64)hDxgi);
        }
    }
#endif // _DEBUG


    PVOID veh_hanle = LI_FN(RtlAddVectoredExceptionHandler)(1, VehHandler);
    if (!veh_hanle) {
        DbgPrint("veh erro !");
        return -1;
    }

    for (size_t idx = 0; idx < ArrNum(hook_offsets); idx++) {

        hook_fun_address = 0;

        HMODULE hDxgi = LI_FN(GetModuleHandleA)("dxgi.dll");

        hook_fun_address = (DWORD64)hDxgi + hook_offsets[idx];

        MEMORY_BASIC_INFORMATION mem_info;
        memset(&mem_info, 0, sizeof(mem_info));
        LI_FN(VirtualQuery)((LPCVOID)hook_fun_address, &mem_info, sizeof(mem_info));
        hook_fun_memory_proct = mem_info.Protect;

        DbgPrint("set hook at 0x%p\t ", hook_fun_address);

        LI_FN(VirtualProtect)((LPVOID)hook_fun_address, 1, mem_info.Protect | PAGE_GUARD, (PDWORD)&hook_fun_memory_proct);

        for (size_t i = 0;; i++) {
            LI_FN(Sleep)(100);
            if (i > 50 ) {
                DbgPrint("hook time out ");
                if (_InterlockedCompareExchange(&hook_fun_execute, 1, 1) == 0) {
                    break;
                }
            }
            if (_InterlockedCompareExchange(&hook_fun_done, 1, 1) == 1)
                break;
        }

        LI_FN(VirtualProtect)((LPVOID)hook_fun_address, 1, mem_info.Protect, (PDWORD)&hook_fun_memory_proct);

        if (_InterlockedCompareExchange(&hook_fun_execute, 1, 1) == 1)
            break;
        else
            continue;
    }

    LI_FN(RtlRemoveVectoredExceptionHandler)(veh_hanle);


    DbgPrint("fun_execute:[%d]\t fun_done:[%d]\t ", _InterlockedCompareExchange(&hook_fun_execute, 1, 1),
             _InterlockedCompareExchange(&hook_fun_done, 1, 1));

    if (_InterlockedCompareExchange(&hook_fun_execute, 1, 1) == 1 &&
        _InterlockedCompareExchange(&hook_fun_done, 1, 1) == 1) {
        return 1;
    }

    return -1;
}


LONG WINAPI VehHandler(EXCEPTION_POINTERS *pExceptionInfo) {

    DWORD64 page_start = ((DWORD64)(hook_fun_address)) & 0xFFFFFFFFFFFFF000;
    DWORD64 page_end   = page_start + 0x1000;

    LONG result;
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION) //
    {
        if ((pExceptionInfo->ContextRecord->Rip >= page_start) && (pExceptionInfo->ContextRecord->Rip <= page_end)) {

            if (pExceptionInfo->ContextRecord->Rip == (DWORD64)(hook_fun_address)) {

                _InterlockedExchange(&hook_fun_execute, 1 );

                pExceptionInfo->ContextRecord->Rip = (DWORD64)&HookFunCallBack;

                return EXCEPTION_CONTINUE_EXECUTION;
            }

            pExceptionInfo->ContextRecord->EFlags |= 0x100;
        }

        result = EXCEPTION_CONTINUE_EXECUTION;
    }

    else if (pExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP) {
        DWORD dwOld;
        LI_FN(VirtualProtect)((LPVOID)hook_fun_address, 1, hook_fun_memory_proct | PAGE_GUARD, &dwOld);
        result = EXCEPTION_CONTINUE_EXECUTION;
    }

    else {
        result = EXCEPTION_CONTINUE_SEARCH;
    }

    return result;
}

__int64 __fastcall HookFunCallBack(IUnknown *pDXGISwapChain, __int64 a2, __int64 a3, __int64 a4, __int64 a5, __int64 a6, __int64 a7,
                                   __int64 a8) {
   
    DbgPrint("HookFunCallBack IUnknown* [0x%p]", pDXGISwapChain);

    auto ret = // ����ԭ hook ����
        reinterpret_cast<decltype(HookFunCallBack) *>(hook_fun_address)(pDXGISwapChain, a2, a3, a4, a5, a6, a7, a8);
    
    //���� dxgi ����
    TakeDxgiCapture(pDXGISwapChain);

    return ret;
}

/*
�ӽ�������ȡD3D�豸�������ġ��ͺ󱸻�������������
�� �ͺ󱸻����� ���������Ǵ����� pCaptureD3D11Texture2D(CPU�ɷ��ʵ�)
map pCaptureD3D11Texture2D Ȼ��bitmap��������
*/
void TakeDxgiCapture(IUnknown *pDXGISwapChain) {

    // ʹ�� comptr ģ���������ֶ����� com��Դ
    Microsoft::WRL::ComPtr<ID3D11Device>        pD3D11Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> pID3D11DeviceContext;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>     pD3D11Texture2D;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>     pCaptureD3D11Texture2D;
    D3D11_TEXTURE2D_DESC                        SwapChanDesc{};

    // ��Ҫֱ��ʹ�� D3D SDK�е� IID_ID3D11Device ��ȫ�ֱ�������Щ����������dxϵ�е� lib�� dll�� 
    _GUID(IID_ID3D11Device, 0xdb6f6ddb, 0xac77, 0x4e88, 0x82, 0x53, 0x81, 0x9d, 0xf9, 0xbb, 0xf1, 0x40);
    auto hr = reinterpret_cast<IDXGISwapChain*>(pDXGISwapChain)
                  ->GetDevice(IID_ID3D11Device, (void **)pD3D11Device.ReleaseAndGetAddressOf());

    if (hr == S_OK) {
        _GUID(IID_ID3D11Texture2D, 0x6f15aaf2, 0xd208, 0x4e89, 0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c);
        hr = reinterpret_cast<IDXGISwapChain *>(pDXGISwapChain)
                 ->GetBuffer(0,IID_ID3D11Texture2D, (void **)pD3D11Texture2D.ReleaseAndGetAddressOf());

        if (hr == S_OK) {
            pD3D11Texture2D->GetDesc(&SwapChanDesc);
            SwapChanDesc.BindFlags      = 0;
            SwapChanDesc.MiscFlags      = 0;
            // CPU�ɷ��ʵ�����
            SwapChanDesc.CPUAccessFlags = 0x30000;
            SwapChanDesc.Usage          = D3D11_USAGE_STAGING;
            hr = pD3D11Device->CreateTexture2D(&SwapChanDesc, 0, pCaptureD3D11Texture2D.ReleaseAndGetAddressOf());

            if (hr == S_OK) {
                pD3D11Device->GetImmediateContext(pID3D11DeviceContext.ReleaseAndGetAddressOf());

                pID3D11DeviceContext->CopyResource(pCaptureD3D11Texture2D.Get(), pD3D11Texture2D.Get());

                D3D11_MAPPED_SUBRESOURCE MappedResource{};
                hr = pID3D11DeviceContext->Map(pCaptureD3D11Texture2D.Get(), 0, D3D11_MAP_READ_WRITE, 0,
                                               &MappedResource);

                if (hr == S_OK) {
                    LPVOID buffer =
                        LI_FN(VirtualAlloc)((LPVOID)0,
                                            static_cast<SIZE_T>(sizeof(D3D11_TEXTURE2D_DESC) +
                                            (static_cast<SIZE_T>(SwapChanDesc.Height) * SwapChanDesc.Width * 0x4)),
                                            MEM_COMMIT,
                                            PAGE_READWRITE);

                    // ������ڴ��� Height * Width * 4  + sizeof(D3D11_TEXTURE2D_DESC) Ҳ���ǰ� descҲֱ��һ���Ƶ���������ڴ��У�����������  dwm-screen-shot.exe��ʡ�˺ܶ��鷳
                    memcpy(buffer, &SwapChanDesc, sizeof(D3D11_TEXTURE2D_DESC));
                    memcpy((char *)buffer + sizeof(D3D11_TEXTURE2D_DESC), MappedResource.pData,
                           (static_cast<SIZE_T>(SwapChanDesc.Height) * SwapChanDesc.Width * 0x4));
                    CaptureBitmapPointer = (__int64)buffer;
                    CaptureWidth         = SwapChanDesc.Width;
                    CaptureHeight        = SwapChanDesc.Height;
                    pID3D11DeviceContext->Unmap(pCaptureD3D11Texture2D.Get(), 0);

                    DbgPrint("Success at 0x%p [ %d * %d ]", CaptureBitmapPointer, CaptureWidth, CaptureHeight);

                    _InterlockedExchange(&hook_fun_done, 1);
                    return;

                } else {
                    goto Fail;
                }
            } else {
                goto Fail;
            }
        } else {
            goto Fail;
        }
    } else {
        goto Fail;
    }

    //���ڹ��̲����ӣ�����ʹ��������ָ�룬�������� goto��
Fail:
    DbgPrint("TakeDxgiCapture Fail !");
    _InterlockedExchange(&hook_fun_done, 1);
}