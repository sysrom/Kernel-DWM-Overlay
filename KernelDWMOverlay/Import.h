#pragma once
#include <Ntifs.h>
#include <WinDef.h>
#include <wingdi.h>

#define MAXMSG_WIDTH	0x100
#define MAXMSG_HEIGHT	0x100
#define GDI_HANDLE_TYPE_MASK  0x007f0000
#define GDI_HANDLE_COUNT 0x10000
#define GDI_HANDLE_INDEX_MASK (GDI_HANDLE_COUNT - 1)
#define GDI_HANDLE_GET_TYPE(h)     \
    (((ULONG_PTR)(h)) & GDI_HANDLE_TYPE_MASK)
#define GDI_HANDLE_GET_INDEX(h)    \
    (((ULONG_PTR)(h)) & GDI_HANDLE_INDEX_MASK)

//_KTHREAD stuff for drawing
ULONG processOffset = 0x220; //_KTHREAD->_KPROCESS* Process;
ULONG win32ThreadOffset = 0x1c8; //_KTHREAD->VOID* volatile Win32Thread

//_ETHREAD stuff for drawing
ULONG cidOffset = 0x478; //_ETHREAD->_CLIENT_ID Cid;  

//EPROC stuff for find eproc by name
ULONG imageFileNameOffset = 0x5a8; //_PEPROCESS->UCHAR ImageFileName[15];   

typedef DWORD LFTYPE;

extern "C"
{
	NTSTATUS NTAPI ZwQuerySystemInformation(ULONG SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);
	NTKERNELAPI PVOID NTAPI RtlFindExportedRoutineByName(PVOID ImageBase, PCCH RoutineNam);
	NTKERNELAPI PVOID NTAPI PsGetCurrentThreadWin32Thread(VOID);
	NTKERNELAPI PCHAR PsGetProcessImageFileName(__in PEPROCESS Process);
	NTKERNELAPI PPEB NTAPI PsGetProcessPeb(IN PEPROCESS Process);
		NTKERNELAPI PPEB NTAPI PsGetProcessWow64Process(IN PEPROCESS Process);
}

typedef enum _SYSTEM_INFORMATION_CLASS
{
	SystemBasicInformation,
	SystemProcessorInformation,
	SystemPerformanceInformation,
	SystemTimeOfDayInformation,
	SystemPathInformation,
	SystemProcessInformation,
	SystemCallCountInformation,
	SystemDeviceInformation,
	SystemProcessorPerformanceInformation,
	SystemFlagsInformation,
	SystemCallTimeInformation,
	SystemModuleInformation,
} SYSTEM_INFORMATION_CLASS, * PSYSTEM_INFORMATION_CLASS;

typedef struct _SYSTEM_MODULE_ENTRY {
	HANDLE Section;
	PVOID MappedBase;
	PVOID ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	UCHAR FullPathName[256];
} SYSTEM_MODULE_ENTRY, * PSYSTEM_MODULE_ENTRY;

typedef struct _SYSTEM_MODULE_INFORMATION {
	ULONG Count;
	SYSTEM_MODULE_ENTRY Module[1];
} SYSTEM_MODULE_INFORMATION, * PSYSTEM_MODULE_INFORMATION;

typedef unsigned short GLYPH;

typedef struct
{
	BOOL	state;
	int		countdown;
	BOOL	started;
	int		runlen;
	int		blippos;
	int		bliplen;
	int		length;
	GLYPH* glyph;

} MATRIX_COLUMN;
typedef struct
{
	WORD	message[MAXMSG_WIDTH][MAXMSG_HEIGHT];
	int		msgindex;
	int		counter;
	WORD	random_reg1;
	int		width, height;
} MATRIX_MESSAGE;

typedef struct
{
	int				width;
	int				height;
	int				numcols;
	int				numrows;
	HDC				hdcBitmap;
	HBITMAP			hbmBitmap;
	MATRIX_MESSAGE* message;
	MATRIX_COLUMN	column[1];
} MATRIX;

typedef FLOAT FLOATOBJ, * PFLOATOBJ;

typedef struct _RGN_ATTR
{
	ULONG AttrFlags;
	ULONG iComplexity;     /* Clipping region's complexity. NULL, SIMPLE & COMPLEXREGION */
	RECTL Rect;
} RGN_ATTR, * PRGN_ATTR;

typedef struct _DC_ATTR
{
	PVOID pvLDC;
	ULONG ulDirty_;
	HANDLE hbrush;
	HANDLE hpen;
	COLORREF crBackgroundClr;
	ULONG ulBackgroundClr;
	COLORREF crForegroundClr;
	ULONG ulForegroundClr;
	COLORREF crBrushClr;
	ULONG ulBrushClr;
	COLORREF crPenClr;
	ULONG ulPenClr;
	DWORD iCS_CP;
	INT iGraphicsMode;
	BYTE jROP2;
	BYTE jBkMode;
	BYTE jFillMode;
	BYTE jStretchBltMode;
	POINTL ptlCurrent;
	POINTL ptfxCurrent;
	LONG lBkMode;
	LONG lFillMode;
	LONG lStretchBltMode;
	FLONG flFontMapper;
	LONG lIcmMode;
	HANDLE hcmXform;
	HCOLORSPACE hColorSpace;
	FLONG flIcmFlags;
	INT IcmBrushColor;
	INT IcmPenColor;
	PVOID pvLIcm;
	FLONG flTextAlign;
	LONG lTextAlign;
	LONG lTextExtra;
	LONG lRelAbs;
	LONG lBreakExtra;
	LONG cBreak;
	HANDLE hlfntNew;
	MATRIX mxWorldToDevice;
	MATRIX mxDeviceToWorld;
	MATRIX mxWorldToPage;
	FLOATOBJ efM11PtoD;
	FLOATOBJ efM22PtoD;
	FLOATOBJ efDxPtoD;
	FLOATOBJ efDyPtoD;
	INT iMapMode;
	DWORD dwLayout;
	LONG lWindowOrgx;
	POINTL ptlWindowOrg;
	SIZEL szlWindowExt;
	POINTL ptlViewportOrg;
	SIZEL szlViewportExt;
	FLONG flXform;
	SIZEL szlVirtualDevicePixel;
	SIZEL szlVirtualDeviceMm;
	SIZEL szlVirtualDeviceSize;
	POINTL ptlBrushOrigin;
	RGN_ATTR VisRectRegion;
} DC_ATTR, * PDC_ATTR;
typedef enum GDILoObjType
{
	GDILoObjType_LO_BRUSH_TYPE = 0x100000,
	GDILoObjType_LO_DC_TYPE = 0x10000,
	GDILoObjType_LO_BITMAP_TYPE = 0x50000,
	GDILoObjType_LO_PALETTE_TYPE = 0x80000,
	GDILoObjType_LO_FONT_TYPE = 0xa0000,
	GDILoObjType_LO_REGION_TYPE = 0x40000,
	GDILoObjType_LO_ICMLCS_TYPE = 0x90000,
	GDILoObjType_LO_CLIENTOBJ_TYPE = 0x60000,
	GDILoObjType_LO_ALTDC_TYPE = 0x210000,
	GDILoObjType_LO_PEN_TYPE = 0x300000,
	GDILoObjType_LO_EXTPEN_TYPE = 0x500000,
	GDILoObjType_LO_DIBSECTION_TYPE = 0x250000,
	GDILoObjType_LO_METAFILE16_TYPE = 0x260000,
	GDILoObjType_LO_METAFILE_TYPE = 0x460000,
	GDILoObjType_LO_METADC16_TYPE = 0x660000
} GDILOOBJTYPE, * PGDILOOBJTYPE;
typedef struct
{
	LPVOID pKernelAddress;
	USHORT wProcessId;
	USHORT wCount;
	USHORT wUpper;
	USHORT wType;
	LPVOID pUserAddress;
} GDICELL;

PVOID NtUserGetDCPtr = 0;
PVOID NtGdiSelectBrushPtr = 0;
PVOID NtGdiPatBltPtr = 0;
PVOID NtUserReleaseDCPtr = 0;
PVOID NtGdiCreateSolidBrushPtr = 0;
PVOID NtGdiDeleteObjectAppPtr = 0;
PVOID NtGdiExtTextOutWPtr = 0;
PVOID NtGdiHfontCreatePtr = 0;
PVOID NtGdiSelectFontPtr = 0;

inline HDC NtUserGetDC(HWND hwnd)
{
	auto fn = reinterpret_cast<HDC(*)(HWND hwnd)>(NtUserGetDCPtr);
	return fn(hwnd);
}

inline HBRUSH NtGdiSelectBrush(HDC hdc, HBRUSH hbrush)
{
	auto fn = reinterpret_cast<HBRUSH(*)(HDC hdc, HBRUSH hbrush)>(NtGdiSelectBrushPtr);
	return fn(hdc, hbrush);
}

inline BOOL NtGdiPatBlt(HDC hdcDest, INT x, INT y, INT cx, INT cy, DWORD dwRop)
{
	auto fn = reinterpret_cast<BOOL(*)(HDC hdcDest, INT x, INT y, INT cx, INT cy, DWORD dwRop)>(NtGdiPatBltPtr);
	return fn(hdcDest, x, y, cx, cy, dwRop);
}

inline int NtUserReleaseDC(HDC hdc)
{
	auto fn = reinterpret_cast<int(*)(HDC hdc)>(NtUserReleaseDCPtr);
	return fn(hdc);
}

inline HBRUSH NtGdiCreateSolidBrush(COLORREF cr, HBRUSH hbr)
{
	auto fn = reinterpret_cast<HBRUSH(*)(COLORREF cr, HBRUSH hbr)>(NtGdiCreateSolidBrushPtr);
	return fn(cr, hbr);
}

inline BOOL NtGdiDeleteObjectApp(HANDLE hobj)
{
	auto fn = reinterpret_cast<BOOL(*)(HANDLE hobj)>(NtGdiDeleteObjectAppPtr);
	return fn(hobj);
}

inline BOOL NtGdiExtTextOutW(HDC hDC, INT XStart, INT YStart, UINT fuOptions, LPRECT UnsafeRect, LPWSTR UnsafeString, INT Count, LPINT UnsafeDx, DWORD dwCodePage)
{
	auto fn = reinterpret_cast<BOOL(*)(HDC hDC, INT XStart, INT YStart, UINT fuOptions, LPRECT UnsafeRect, LPWSTR UnsafeString, INT Count, LPINT UnsafeDx, DWORD dwCodePage)>(NtGdiExtTextOutWPtr);
	return fn(hDC, XStart, YStart, fuOptions, UnsafeRect, UnsafeString, Count, UnsafeDx, dwCodePage);
}

inline HFONT NtGdiHfontCreate(PENUMLOGFONTEXDVW pelfw, ULONG cjElfw, LFTYPE lft, FLONG fl, PVOID pvCliData)
{
	auto fn = reinterpret_cast<HFONT(*)(PENUMLOGFONTEXDVW pelfw, ULONG cjElfw, LFTYPE lft, FLONG fl, PVOID pvCliData)>(NtGdiHfontCreatePtr);
	return fn(pelfw, cjElfw, lft, fl, pvCliData);
}

inline HFONT NtGdiSelectFont(HDC hdc, HFONT hfont)
{
	auto fn = reinterpret_cast<HFONT(*)(HDC hdc, HFONT hfont)>(NtGdiSelectFontPtr);
	return fn(hdc, hfont);
}


