#include "Overlay.h"



void MainThread()
{
	DbgPrintEx(0, 0, "[sysR@M]> Main Thread Started.\n");
	currentProcess = IoGetCurrentProcess();
	currentThread = KeGetCurrentThread();
	memcpy(&currentCid, (PVOID)((char*)currentThread + cidOffset), sizeof(CLIENT_ID));

	while (true)
	{
		Overlay::BeginDraw();

		Overlay::DrawFect({ 100, 100, 200, 200 }, 3);
		Overlay::DrawText(200, 200, 0, NULL, "KernelOverlay", strlen("KernelOverlay"), NULL);
		Overlay::EndDraw();

		YieldProcessor();
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
	if (Overlay::Init()==STATUS_SUCCESS) {
		DbgPrintEx(0, 0, "[sysR@M]> Overlay Init SUCCESS\n");
	}
	else {
		DbgPrintEx(0, 0, "[sysR@M]> Overlay Init FAILED\n");
		return STATUS_UNSUCCESSFUL;
	}
	CreateThread(MainThread);
	
	return STATUS_SUCCESS;
}
