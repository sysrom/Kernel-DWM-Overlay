#include "Overlay.h"

extern "C" {
	NTKERNELAPI PCHAR PsGetProcessImageFileName(__in PEPROCESS Process);
}
inline PEPROCESS DWM_Process;
inline KAPC_STATE DWM_apc_state;

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
		KdPrint(("GetActiveProcessLinksOffset failed\n"));
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
		DbgPrintEx(0,0,"[sysR@M]> Failed to Get DWM PROCESS");
		return false;
	}
	KeStackAttachProcess(DWM_Process,&DWM_apc_state);
	return true;
}

BOOLEAN DetachDWM() { //for UCMapper
	KeUnstackDetachProcess(&DWM_apc_state);
	return true;
}

void MainThread()
{
	DbgPrintEx(0, 0, "[sysR@M]> Main Thread Started.\n");
	currentProcess = IoGetCurrentProcess();
	currentThread = KeGetCurrentThread();
	memcpy(&currentCid, (PVOID)((char*)currentThread + cidOffset), sizeof(CLIENT_ID));

	while (true)
	{
		BeginDraw();

		DrawFect({ 100, 100, 200, 200 }, 3);

		EndDraw();
	}

	PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS CreateThread(PVOID entry)
{
	HANDLE threadHandle = NULL;
	NTSTATUS status = PsCreateSystemThread(&threadHandle, NULL, NULL, NULL, NULL, (PKSTART_ROUTINE)entry, NULL);

	if (!NT_SUCCESS(status))
	{
		DbgPrintEx(0, 0, "[sysR@M]> Failed to create system thread:%x\n", status);
		return status;
	}

	ZwClose(threadHandle);
	return status;
}

extern "C" NTSTATUS DriverEntry()
{
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
		return false;
	}

	CreateThread(MainThread);
	
	return STATUS_SUCCESS;
}
