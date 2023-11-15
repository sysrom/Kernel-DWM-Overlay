#pragma once
#include "Misc.h"

KAPC_STATE apc = { 0 };
PVOID currentWin32Thread = 0;
PEPROCESS currentProcess = 0;
PETHREAD currentThread = 0;
CLIENT_ID currentCid = { 0 };
HDC hdc;
HBRUSH brush;

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