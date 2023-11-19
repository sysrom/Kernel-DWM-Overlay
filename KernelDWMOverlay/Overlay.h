#pragma once
#include "Misc.h"

KAPC_STATE apc = { 0 };
PVOID currentWin32Thread = 0;
PEPROCESS currentProcess = 0;
PETHREAD currentThread = 0;
CLIENT_ID currentCid = { 0 };
HDC hdc;
HBRUSH brush;

extern "C" {
	NTKERNELAPI PCHAR PsGetProcessImageFileName(__in PEPROCESS Process);
}
inline PEPROCESS DWM_Process;
inline KAPC_STATE DWM_apc_state;


PETHREAD GetWin32Thread(PVOID* win32Thread)
{
	int currentThreadId = 1;
	NTSTATUS status = STATUS_SUCCESS;
	do
	{
		PETHREAD currentEthread = 0;
		status = PsLookupThreadByThreadId((HANDLE)currentThreadId, &currentEthread);

		if (!NT_SUCCESS(status) || !currentEthread)
		{
			currentThreadId++;
			continue;
		}

		if (PsIsThreadTerminating(currentEthread))
		{
			currentThreadId++;
			continue;
		}

		PVOID Win32Thread;
		memcpy(&Win32Thread, (PVOID)((UINT64)currentEthread + win32ThreadOffset), sizeof(PVOID));

		if (Win32Thread)
		{
			PEPROCESS threadOwner = PsGetThreadProcess(currentEthread);
			char procName[15];
			memcpy(&procName, (PVOID)((UINT64)threadOwner + imageFileNameOffset), sizeof(procName));
			if (!strcmp(procName, "dwm.exe"))
			{
				*win32Thread = Win32Thread;
				return currentEthread;
			}
		}
		currentThreadId++;
	} while (0x3000 > currentThreadId);

	return 0;
}

inline void SpoofGuiThread(PVOID newWin32Value, PEPROCESS newProcess, CLIENT_ID newClientId)
{
	PKTHREAD currentThread = KeGetCurrentThread();

	PVOID win32ThreadPtr = (PVOID)((char*)currentThread + win32ThreadOffset);
	memcpy(win32ThreadPtr, &newWin32Value, sizeof(PVOID));

	PVOID processPtr = (PVOID)((char*)currentThread + processOffset);
	memcpy(processPtr, &newProcess, sizeof(PEPROCESS));

	PVOID clientIdPtr = (PVOID)((char*)currentThread + cidOffset);
	memcpy(clientIdPtr, &newClientId, sizeof(CLIENT_ID));
}

ULONG GetActiveProcessLinksOffset()
{
	UNICODE_STRING FunName = { 0 };
	RtlInitUnicodeString(&FunName, L"PsGetProcessId");
	PUCHAR pfnPsGetProcessId = (PUCHAR)MmGetSystemRoutineAddress(&FunName);
	if (pfnPsGetProcessId && MmIsAddressValid(pfnPsGetProcessId) && MmIsAddressValid(pfnPsGetProcessId + 0x7))
	{
		for (size_t i = 0; i < 0x7; i++)
		{
			if (pfnPsGetProcessId[i] == 0x48 && pfnPsGetProcessId[i + 1] == 0x8B)
			{
				return *(PULONG)(pfnPsGetProcessId + i + 3) + 8;
			}
		}
	}
	return 0;
}

PEPROCESS GetProcessByName(const char* szName)
{
	PEPROCESS Process = NULL;
	PCHAR ProcessName = NULL;
	PLIST_ENTRY pHead = NULL;
	PLIST_ENTRY pNode = NULL;

	ULONG64 ActiveProcessLinksOffset = GetActiveProcessLinksOffset();
	//KdPrint(("ActiveProcessLinksOffset = %llX\n", ActiveProcessLinksOffset));
	if (!ActiveProcessLinksOffset)
	{
		DbgPrintEx(0, 0, "[sysR@M]> GetActiveProcessLinksOffset failed\n");
		return NULL;
	}
	Process = PsGetCurrentProcess();

	pHead = (PLIST_ENTRY)((ULONG64)Process + ActiveProcessLinksOffset);
	pNode = pHead;

	do
	{
		Process = (PEPROCESS)((ULONG64)pNode - ActiveProcessLinksOffset);
		ProcessName = PsGetProcessImageFileName(Process);
		//KdPrint(("%s\n", ProcessName));
		if (!strcmp(szName, ProcessName))
		{
			return Process;
		}

		pNode = pNode->Flink;
	} while (pNode != pHead);

	return NULL;
}


BOOLEAN AttachDWM() { //for UCMapper
	DWM_Process = GetProcessByName("dwm.exe");
	if (DWM_Process == NULL) {
		DbgPrintEx(0, 0, "[sysR@M]> Failed to Get DWM PROCESS");
		return false;
	}
	KeStackAttachProcess(DWM_Process, &DWM_apc_state);
	return true;
}

BOOLEAN DetachDWM() { //for UCMapper
	KeUnstackDetachProcess(&DWM_apc_state);
	return true;
}

namespace Overlay{
	NTSTATUS Init() {
		DbgPrintEx(0, 0, "[sysR@M]> Call Attach DWM.\n");
		if (AttachDWM()) {
			DbgPrintEx(0, 0, "[sysR@M]> SUCCRSS to attach DWM\n");
		}
		else {
			DbgPrintEx(0, 0, "[sysR@M]> Failed to attach DWM\n");
			return STATUS_UNSUCCESSFUL;
		}
		DbgPrintEx(0, 0, "[sysR@M]> Called AttachDWM\n");

		NTSTATUS status = STATUS_SUCCESS;


		PVOID win32kBase = (PVOID)GetKernelModuleBase("win32kbase.sys");
		PVOID win32kfullBase = (PVOID)GetKernelModuleBase("win32kfull.sys");

		if (!win32kBase || !win32kfullBase)
		{
			DbgPrintEx(0, 0, "[sysR@M]> Could not find kernel module bases\n");
			return STATUS_UNSUCCESSFUL;
		}

		NtUserGetDCPtr = RtlFindExportedRoutineByName(win32kBase, "NtUserGetDC");
		NtGdiPatBltPtr = RtlFindExportedRoutineByName(win32kfullBase, "NtGdiPatBlt");
		NtGdiSelectBrushPtr = RtlFindExportedRoutineByName(win32kBase, "GreSelectBrush");
		NtUserReleaseDCPtr = RtlFindExportedRoutineByName(win32kBase, "NtUserReleaseDC");
		NtGdiCreateSolidBrushPtr = RtlFindExportedRoutineByName(win32kfullBase, "NtGdiCreateSolidBrush");
		NtGdiDeleteObjectAppPtr = RtlFindExportedRoutineByName(win32kBase, "NtGdiDeleteObjectApp");
		NtGdiExtTextOutWPtr = RtlFindExportedRoutineByName(win32kfullBase, "NtGdiExtTextOutW");
		NtGdiHfontCreatePtr = RtlFindExportedRoutineByName(win32kfullBase, "hfontCreate");
		NtGdiSelectFontPtr = RtlFindExportedRoutineByName(win32kfullBase, "NtGdiSelectFont");


		if (!NtUserGetDCPtr || !NtGdiPatBltPtr || !NtGdiSelectBrushPtr ||
			!NtUserReleaseDCPtr || !NtGdiCreateSolidBrushPtr || !NtGdiDeleteObjectAppPtr
			|| !NtGdiExtTextOutWPtr || !NtGdiHfontCreatePtr || !NtGdiSelectFontPtr)
		{
			DbgPrintEx(0, 0, "[sysR@M]> Could not find kernel functions required for drawing\n");
			return STATUS_UNSUCCESSFUL;
		}

		if (DetachDWM()) {
			DbgPrintEx(0, 0, "[sysR@M]> SUCCESS to Detach DWM\n");
		}
		else {
			DbgPrintEx(0, 0, "[sysR@M]> Failed to Detach DWM\n");
			return STATUS_UNSUCCESSFUL;
		}
		return STATUS_SUCCESS;
	}
	bool BeginDraw()
	{
		PVOID targetWin32Thread = 0;
		PETHREAD targetThread = GetWin32Thread(&targetWin32Thread);
		if (!targetWin32Thread || !targetThread)
		{
			DbgPrintEx(0, 0, "[sysR@M]> Failed to find win32thread");
			return false;
		}
		PEPROCESS targetProcess = PsGetThreadProcess(targetThread);

		CLIENT_ID targetCid = { 0 };
		memcpy(&targetCid, (PVOID)((char*)targetThread + cidOffset), sizeof(CLIENT_ID));

		KeStackAttachProcess(targetProcess, &apc);
		SpoofGuiThread(targetWin32Thread, targetProcess, targetCid);
		hdc = NtUserGetDC(0);
		if (!hdc)
		{
			DbgPrintEx(0, 0, "[sysR@M]> Failed to get userdc");
			return false;
		}
		brush = NtGdiCreateSolidBrush(RGB(255, 0, 0), NULL);
		if (!brush)
		{
			DbgPrintEx(0, 0, "[sysR@M]> Failed create brush");
			NtUserReleaseDC(hdc);
			return false;
		}
	}

	void EndDraw()
	{
		NtGdiDeleteObjectApp(brush);
		NtUserReleaseDC(hdc);

		SpoofGuiThread(currentWin32Thread, currentProcess, currentCid);
		KeUnstackDetachProcess(&apc);
	}

	INT DrawFect(RECT rect, int thickness)
	{
		HBRUSH oldBrush = NtGdiSelectBrush(hdc, brush);
		if (!oldBrush)
		{
			DbgPrintEx(0, 0, "[sysR@M]> Failed to get brush");
			return 0;
		}


		NtGdiPatBlt(hdc, rect.left, rect.top, thickness, rect.bottom - rect.top, PATCOPY);
		NtGdiPatBlt(hdc, rect.right - thickness, rect.top, thickness, rect.bottom - rect.top, PATCOPY);
		NtGdiPatBlt(hdc, rect.left, rect.top, rect.right - rect.left, thickness, PATCOPY);
		NtGdiPatBlt(hdc, rect.left, rect.bottom - thickness, rect.right - rect.left, thickness, PATCOPY);

		NtGdiSelectBrush(hdc, oldBrush);
		return 1;
	}

	// thanks https://github.com/BadPlayer555/KernelGDIDraw/blob/master/Test2/Test2/main.cpp#L166
	BOOL ExtTextOutW(HDC hdc_Func, INT x, INT y, UINT fuOptions, RECT* lprc, LPWSTR lpString, UINT cwc, INT* lpDx)
	{
		BOOL		nRet = FALSE;
		PVOID       local_lpString = NULL;
		RECT* local_lprc = NULL;
		INT* local_lpDx = NULL;

		if (lprc != NULL)
		{
			SIZE_T Len = sizeof(RECT);
			local_lprc = (RECT*)AllocateVirtualMemory(Len);
			if (local_lprc != NULL)
			{
				__try
				{
					RtlZeroMemory(local_lprc, Len);
					RtlCopyMemory(local_lprc, lprc, Len);
				}
				__except (1)
				{
					DbgPrintEx(0,0,"[sysR@M]> ExtTextOutW->RtlCopyMemory Failed!");
					goto $EXIT;
				}
			}
			else
			{
				DbgPrintEx(0, 0, "[sysR@M]> ExtTextOutW->local_lprc = null");
				goto $EXIT;
			}
		}
		if (cwc != 0)
		{
			SIZE_T     AllocSize = sizeof(WCHAR) * cwc + 1;
			local_lpString = AllocateVirtualMemory(AllocSize);
			if (local_lpString != NULL)
			{
				__try
				{
					RtlZeroMemory(local_lpString, AllocSize);
					RtlCopyMemory(local_lpString, lpString, AllocSize);
				}
				__except (1)
				{
					DbgPrintEx(0, 0, "[sysR@M]> local_lpString->RtlCopyMemory Failed!");
					goto $EXIT;
				}
			}
			else
			{
				DbgPrintEx(0, 0, "[sysR@M]> local_lpString=null");
				goto $EXIT;
			}
		}
		if (local_lpDx != NULL)
		{
			SIZE_T     AllocSize = sizeof(INT);
			local_lpDx = (INT*)AllocateVirtualMemory(AllocSize);
			if (local_lpDx != NULL)
			{
				__try
				{
					RtlZeroMemory(local_lpString, AllocSize);
					*local_lpDx = *lpDx;
				}
				__except (1)
				{
					DbgPrintEx(0, 0, "[sysR@M]> local_lpDx->RtlCopyMemory");
					goto $EXIT;
				}
			}
			else
			{
				DbgPrintEx(0, 0, "[sysR@M]> local_lpDx = null");
			}
		}


		if (NtGdiExtTextOutW != NULL) {
			nRet = NtGdiExtTextOutW(hdc_Func, x, y, fuOptions, local_lprc, (LPWSTR)local_lpString, cwc, local_lpDx, 0);
		}
		else {
			DbgPrintEx(0, 0, "[sysR@M]> Call NtGdiExtTextOutW Failed");
		}
	$EXIT:
		if (lprc != NULL)
		{
			FreeVirtualMemory(lprc, sizeof(RECT));
			lprc = NULL;
		}

		if (local_lpDx != NULL)
		{
			FreeVirtualMemory(local_lpDx, sizeof(INT));
			local_lpDx = NULL;
		}

		if (local_lpString != NULL)
		{
			FreeVirtualMemory(local_lpString, cwc);
			local_lpString = NULL;
		}
		return nRet;
	}

	BOOL DrawText(INT x, INT y, UINT fuOptions, RECT* lprc, LPCSTR lpString, UINT cch, INT* lpDx)
	{
		ANSI_STRING StringA;
		UNICODE_STRING StringU;
		BOOL ret;
		RtlInitAnsiString(&StringA, (LPSTR)lpString);
		RtlAnsiStringToUnicodeString(&StringU, &StringA, TRUE);
		ret = ExtTextOutW(hdc, x, y, fuOptions, lprc, StringU.Buffer, cch, lpDx);
		RtlFreeUnicodeString(&StringU);
		return ret;
	}
}