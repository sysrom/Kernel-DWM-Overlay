#pragma once
#include "Import.h"

void Sleep(int ms)
{
	LARGE_INTEGER time = { 0 };
	time.QuadPart = -(ms) * 10 * 1000;
	KeDelayExecutionThread(KernelMode, TRUE, &time);
}

PVOID QuerySystemInformation(SYSTEM_INFORMATION_CLASS SystemInfoClass, ULONG* size)
{
	int currAttempt = 0;
	int maxAttempt = 20;


QueryTry:
	if (currAttempt >= maxAttempt)
		return 0;

	currAttempt++;
	ULONG neededSize = 0;
	ZwQuerySystemInformation(SystemInfoClass, NULL, neededSize, &neededSize);
	if (!neededSize)
		goto QueryTry;

	ULONG allocationSize = neededSize;
	PVOID informationBuffer = ExAllocatePool(NonPagedPool ,allocationSize);
	if (!informationBuffer)
		goto QueryTry;

	NTSTATUS status = ZwQuerySystemInformation(SystemInfoClass, informationBuffer, neededSize, &neededSize);
	if (!NT_SUCCESS(status))
	{
		ExFreePoolWithTag(informationBuffer, 0);
		goto QueryTry;
	}

	*size = allocationSize;
	return informationBuffer;
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
UINT64 GetKernelModuleBase(const char* name)
{
	ULONG size = 0;
	PSYSTEM_MODULE_INFORMATION moduleInformation = (PSYSTEM_MODULE_INFORMATION)QuerySystemInformation(SystemModuleInformation, &size);

	if (!moduleInformation || !size)
		return 0;

	for (size_t i = 0; i < moduleInformation->Count; i++)
	{
		char* fileName = (char*)moduleInformation->Module[i].FullPathName + moduleInformation->Module[i].OffsetToFileName;
		if (!strcmp(fileName, name))
		{
			UINT64 imageBase = (UINT64)moduleInformation->Module[i].ImageBase;
			ExFreePoolWithTag(moduleInformation, 0);
			return imageBase;
		}
	}

	ExFreePoolWithTag(moduleInformation, 0);
}

BOOL GdiGetHandleUserData(HGDIOBJ hGdiObj, DWORD ObjectType, PVOID* UserData)
{
	PEPROCESS Eprocess = GetProcessByName("dwm.exe");
	PVOID pPeb = PsGetProcessPeb(Eprocess);
	if (pPeb == NULL)
	{
		pPeb = PsGetProcessWow64Process(Eprocess);
		if (pPeb == NULL)
		{
			DbgPrintEx(0,0,"[sysR@M]> GetProcessPEB Failed!");
			return false;
		}
	}
	GDICELL* Entry = (GDICELL*)*(LPVOID*)((ULONG64)pPeb + 0xF8); //GdiSharedHandleTable
	if (Entry == NULL) {
		DbgPrintEx(0, 0, "[sysR@M]> GdiGetUserData.Entry == NULL!");
		return FALSE;
	}
	Entry = Entry + GDI_HANDLE_GET_INDEX(hGdiObj);
	if (Entry == NULL) {
		DbgPrintEx(0, 0, "[sysR@M]> GdiGetUserData.Entry.HANDLE == NULL!");
		return FALSE;
	}
	if (!MmIsAddressValid(Entry->pUserAddress)) {
		DbgPrintEx(0, 0, "[sysR@M]> GdiGetUserData.Entry.pUserAddress Invalid!");
		return FALSE;
	}
	*UserData = Entry->pUserAddress;
}

PDC_ATTR GdiGetDcAttr(HDC hdc)
{
	GDILOOBJTYPE eDcObjType;
	PDC_ATTR pdcattr;
	eDcObjType = (GDILOOBJTYPE)GDI_HANDLE_GET_TYPE(hdc); //ok
	if ((eDcObjType != GDILoObjType_LO_DC_TYPE) &&
		(eDcObjType != GDILoObjType_LO_ALTDC_TYPE))
	{
		return NULL;
	}
	if (!GdiGetHandleUserData((HGDIOBJ)hdc, eDcObjType, (PVOID*)&pdcattr))
	{
		return NULL;
	}
	return pdcattr;
}
PVOID AllocateVirtualMemory(SIZE_T Size)
{
	PVOID pMem = NULL;
	NTSTATUS statusAlloc = ZwAllocateVirtualMemory(NtCurrentProcess(), &pMem, 0, &Size, MEM_COMMIT, PAGE_READWRITE);
	return pMem;
}

VOID FreeVirtualMemory(PVOID VirtualAddress, SIZE_T Size)
{
	if (MmIsAddressValid(VirtualAddress))
	{
		NTSTATUS Status = ZwFreeVirtualMemory(NtCurrentProcess(), &VirtualAddress, &Size, MEM_RELEASE);

		if (!NT_SUCCESS(Status)) {
			DbgPrintEx(0,0,"[sysR@M]> FreeVirtualMemory Failed!");
		}
		return;
	}
}
