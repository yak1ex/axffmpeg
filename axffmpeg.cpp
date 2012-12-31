/***********************************************************************/
/*                                                                     */
/* axffmpeg.cpp: Source file for axffmpeg                              */
/*                                                                     */
/*     Copyright (C) 2012 Yak! / Yasutaka ATARASHI                     */
/*                                                                     */
/*     This software is distributed under the terms of a zlib/libpng   */
/*     License.                                                        */
/*                                                                     */
/*     $Id: 61260feeb364cbc8e9b6daea239472ade36e8dc8 $                 */
/*                                                                     */
/***********************************************************************/
#include <windows.h>
#include <commctrl.h>

#include <string>
#include <vector>
#include <cstdlib>

#include "Spi_api.h"
#include "resource.h"
#ifdef DEBUG
#include <iostream>
#include <iomanip>
#include "odstream.hpp"
#define DEBUG_LOG(ARG) yak::debug::ods ARG
#else
#define DEBUG_LOG(ARG) do { } while(0)
#endif

// Force null terminating version of strncpy
// Return length without null terminator
int safe_strncpy(char *dest, const char *src, std::size_t n)
{
	size_t i = 0;
	while(i<n && *src) { *dest++ = *src++; ++i; }
	*dest = 0;
	return i;
}

const char* table[] = {
	"00AM",
	"Plugin to handle a movie as an image container - v0.01 (201x/xx/xx) Written by Yak!",
	"*.3g2;*.3gp;*.avi;*.flv;*.m4v;*.mov;*.mp4;*.mpeg;*.mpg;*.ogg;*.swf;*.webm;*.wmv",
	"動画ファイル"
};

INT PASCAL GetPluginInfo(INT infono, LPSTR buf, INT buflen)
{
	DEBUG_LOG(<< "GetPluginInfo(" << infono << ',' << buf << ',' << buflen << ')' << std::endl);
	if(0 <= infono && static_cast<size_t>(infono) < sizeof(table)/sizeof(table[0])) {
		return safe_strncpy(buf, table[infono], buflen);
	} else {
		return 0;
	}
}

static INT IsSupportedImp(LPSTR filename, LPBYTE pb)
{
	std::string name(filename);
	const char* start = table[2];
	while(1) {
		while(*start && *start != '.') {
			++start;
		}
		if(!*start) break;
		const char* end = start;
		while(*end && *end != ';') {
			++end;
		}
		std::string ext(start, end);
		if(name.size() <= ext.size()) continue;
		if(!lstrcmpi(name.substr(name.size() - ext.size()).c_str(), ext.c_str())) {
			return SPI_SUPPORT_YES;
		}
		++start;
	}
	return SPI_SUPPORT_NO;
}

INT PASCAL IsSupported(LPSTR filename, DWORD dw)
{
	DEBUG_LOG(<< "IsSupported(" << filename << ',' << std::hex << std::setw(8) << std::setfill('0') << dw << ')' << std::endl);
	if(HIWORD(dw) == 0) {
		DEBUG_LOG(<< "File handle" << std::endl);
		BYTE pb[2048];
		DWORD dwSize;
		ReadFile((HANDLE)dw, pb, sizeof(pb), &dwSize, NULL);
		return IsSupportedImp(filename, pb);;
	} else {
		DEBUG_LOG(<< "Pointer" << std::endl); // By afx
		return IsSupportedImp(filename, (LPBYTE)dw);;
	}
	// not reached here
}

INT GetArchiveInfoImp(LPSTR buf, DWORD len, HLOCAL *lphInf, LPSTR filename = NULL)
{
//ods << "GetArchiveInfoImp(" << std::string(buf, std::min<DWORD>(len, 1024)) << ',' << len << ',' << lphInf << (filename ? filename : "NULL") << ')' << std::endl;

	try {
	} catch(std::exception &e) {
		MessageBox(NULL, e.what(), "axffmpeg.spi", MB_OK);
		return SPI_ERR_BROKEN_DATA;
	} catch(...) {
		return SPI_ERR_BROKEN_DATA;
	}

	return SPI_ERR_NO_ERROR;
}

static std::string g_sFFprobePath;
static std::string g_sFFmpegPath;

static std::pair<HANDLE, HANDLE> InvokeProcess(const std::string &sCommandLine)
{
	STARTUPINFO si = { sizeof(STARTUPINFO) };
	HANDLE hRead1, hWrite1, hRead2, hWrite2;
	SECURITY_ATTRIBUTES saAttr; 
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
	saAttr.bInheritHandle = TRUE; 
	saAttr.lpSecurityDescriptor = NULL; 
	CreatePipe(&hRead1, &hWrite1, &saAttr, 0);
	SetHandleInformation(hRead1, HANDLE_FLAG_INHERIT, 0);
	CreatePipe(&hRead2, &hWrite2, &saAttr, 0);
	SetHandleInformation(hWrite2, HANDLE_FLAG_INHERIT, 0);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = hRead2;
	si.hStdOutput = hWrite1;
	si.hStdError = hWrite1;
	PROCESS_INFORMATION pi;
	std::vector<char> vCommandLine(sCommandLine.begin(), sCommandLine.end());
	vCommandLine.push_back(0);
	if(CreateProcess(0, &vCommandLine[0], 0, 0, TRUE, CREATE_NO_WINDOW, 0, 0, &si, &pi)) {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		CloseHandle(hWrite1);
		CloseHandle(hRead2);
		return std::make_pair(hRead1, hWrite2);
	}
	return std::pair<HANDLE, HANDLE>(0, 0);
}

static unsigned long GetDurationByFile(LPSTR filename)
{
	std::string s = std::string("\"") + g_sFFprobePath + "\" \"" + filename + "\" -v warning -show_entries format=duration -of csv";
	std::pair<HANDLE, HANDLE> handle_pair = InvokeProcess(s);
	unsigned long duration = 0;
	if(handle_pair.first) {
		std::string s;
		DWORD dwLen;
		std::vector<char> buf(2048);
		while(ReadFile(handle_pair.first, &buf[0], buf.size(), &dwLen, 0)) {
			s += std::string(&buf[0], &buf[0] + dwLen);
		}
		if(dwLen) s += std::string(&buf[0], &buf[0] + dwLen);
		const char* p = s.c_str();
		while(*p && *p != ',') ++p;
		if(*p) ++p;
		duration = std::strtoul(p, 0, 10);
		CloseHandle(handle_pair.first);
		CloseHandle(handle_pair.second);
	}
	DEBUG_LOG(<< "GetDurationByFile(" << filename << ") : " << duration << std::endl);
	return duration;
}

INT PASCAL GetArchiveInfo(LPSTR buf, LONG len, UINT flag, HLOCAL *lphInf)
{
	DEBUG_LOG(<< "GetArchiveInfo(" << std::string(buf, std::min<DWORD>(len, 1024)) << ',' << len << ',' << std::hex << std::setw(8) << std::setfill('0') << flag << ',' << lphInf << ')' << std::endl);
	switch(flag & 7) {
	case 0:
		DEBUG_LOG(<< "File" << std::endl); // By afx
		return GetArchiveInfoImp(lphInf, buf);
	case 1:
		DEBUG_LOG(<< "Memory" << std::endl);
		return SPI_ERR_NOT_IMPLEMENTED;
	}
	return SPI_ERR_INTERNAL_ERROR;
}

INT PASCAL GetFileInfo(LPSTR buf, LONG len, LPSTR filename, UINT flag, SPI_FILEINFO *lpInfo)
{
	DEBUG_LOG(<< "GetFileInfo(" << std::string(buf, std::min<DWORD>(len, 1024)) << ',' << len << ',' << filename << ','<< std::hex << std::setw(8) << std::setfill('0') << flag << ',' << lpInfo << ')' << std::endl);
	if(flag & 128) {
		DEBUG_LOG(<< "Case-insensitive" << std::endl); // By afx
	} else {
		DEBUG_LOG(<< "Case-sensitive" << std::endl);
	}
	switch(flag & 7) {
	case 0:
		DEBUG_LOG(<< "File" << std::endl); // By afx
		break;
	case 1:
		DEBUG_LOG(<< "Memory" << std::endl);
		break;
	}
	HLOCAL hInfo;
	if(GetArchiveInfo(buf, len, flag&7, &hInfo) == SPI_ERR_NO_ERROR) {
		int (WINAPI *compare)(LPCSTR, LPCSTR) = flag&7 ? lstrcmpi : lstrcmp;
		std::size_t size = LocalSize(hInfo);
		SPI_FILEINFO *p = static_cast<SPI_FILEINFO*>(LocalLock(hInfo)), *cur = p;
		std::size_t count = size / sizeof(SPI_FILEINFO);
		while(cur < p + count && *static_cast<char*>(static_cast<void*>(cur)) != '\0') {
			if(!compare(cur->filename, filename)) {
				CopyMemory(lpInfo, cur, sizeof(SPI_FILEINFO));
				LocalUnlock(hInfo);
				LocalFree(hInfo);
				return SPI_ERR_NO_ERROR;
			}
			++cur;
		}
		LocalUnlock(hInfo);
		LocalFree(hInfo);
	}
	return SPI_ERR_INTERNAL_ERROR;
}

INT PASCAL GetFile(LPSTR buf, LONG len, LPSTR dest, UINT flag, FARPROC prgressCallback, LONG lData)
{
	DEBUG_LOG(<< "GetFile(" << ',' << len << ',' << ',' << flag << ',' << prgressCallback << ',' << lData << ')' << std::endl);
	switch(flag & 7) {
	case 0:
	  {
		DEBUG_LOG(<< "Source is file" << std::endl); // By afx
		break;
	  }
	case 1:
		DEBUG_LOG(<< "Source is memory" << std::endl);
		return SPI_ERR_NOT_IMPLEMENTED; // We cannot handle this case.
	}
	switch((flag>>8) & 7) {
	case 0:
		DEBUG_LOG(<< "Destination is file: " << dest << std::endl);
		break;
	case 1:
		DEBUG_LOG(<< "Destination is memory: " << reinterpret_cast<void*>(dest) << std::endl); // By afx
		break;
	}
	if(GetArchiveInfo(buf, len, flag&7, 0) == SPI_ERR_NO_ERROR) {
		DEBUG_LOG(<< "GetFile(): GetArvhiveInfo() returned" << std::endl);
		return SPI_ERR_NOT_IMPLEMENTED;
	}
	DEBUG_LOG(<< "GetFile(): position not found" << std::endl);
	return SPI_ERR_INTERNAL_ERROR;
}

static LRESULT CALLBACK AboutDlgProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
		case WM_INITDIALOG:
			return FALSE;
		case WM_COMMAND:
			switch (LOWORD(wp)) {
				case IDOK:
					EndDialog(hDlgWnd, IDOK);
					break;
				case IDCANCEL:
					EndDialog(hDlgWnd, IDCANCEL);
					break;
				default:
					return FALSE;
			}
		default:
			return FALSE;
	}
	return TRUE;
}

static std::string g_sIniFileName; // ini ファイル名
static int g_nNumber;
static int g_fImages;

void LoadFromIni()
{
	g_nNumber = GetPrivateProfileInt("axffmpeg", "number", 1, g_sIniFileName.c_str());
	g_fImages = GetPrivateProfileInt("axffmpeg", "images", 1, g_sIniFileName.c_str());
	std::vector<char> vBuf(1024);
	DWORD dwSize;
	do {
		vBuf.resize(vBuf.size() * 2);
		dwSize = GetPrivateProfileString("axffmpeg", "ffprobe", "", &vBuf[0], vBuf.size(), g_sIniFileName.c_str());
	} while(dwSize == vBuf.size() - 1);
	g_sFFprobePath = std::string(&vBuf[0]);
	do {
		vBuf.resize(vBuf.size() * 2);
		dwSize = GetPrivateProfileString("axffmpeg", "ffmpeg", "", &vBuf[0], vBuf.size(), g_sIniFileName.c_str());
	} while(dwSize == vBuf.size() - 1);
	g_sFFmpegPath = std::string(&vBuf[0]);
}

void SaveToIni()
{
	char buf[1024];
	wsprintf(buf, "%d", g_nNumber);
	WritePrivateProfileString("axffmpeg", "number", buf, g_sIniFileName.c_str());
	WritePrivateProfileString("axffmpeg", "images", g_fImages ? "1" : "0", g_sIniFileName.c_str());
	WritePrivateProfileString("axffmpeg", "ffprobe", g_sFFprobePath.c_str(), g_sIniFileName.c_str());
	WritePrivateProfileString("axffmpeg", "ffmpeg", g_sFFmpegPath.c_str(), g_sIniFileName.c_str());
}

void SetIniFileName(HANDLE hModule)
{
    std::vector<char> vModulePath(1024);
    size_t nLen = GetModuleFileName((HMODULE)hModule, &vModulePath[0], (DWORD)vModulePath.size());
    vModulePath.resize(nLen + 1);
    // 本来は2バイト文字対策が必要だが、プラグイン名に日本語はないと前提として手抜き
    while (!vModulePath.empty() && vModulePath.back() != '\\') {
        vModulePath.pop_back();
    }

    g_sIniFileName = &vModulePath[0];
    g_sIniFileName +=".ini";
}

void UpdateDialogItem(HWND hDlgWnd)
{
	SendDlgItemMessage(hDlgWnd, IDC_SPIN_NUMBER, UDM_SETRANGE32, 0, 0x7FFFFFFF);
	SendDlgItemMessage(hDlgWnd, IDC_SPIN_NUMBER, UDM_SETPOS32, 0, g_nNumber);
	SendDlgItemMessage(hDlgWnd, g_fImages ? IDC_RADIO_IMAGES : IDC_RADIO_INTERVAL, BM_SETCHECK, BST_CHECKED, 0);
	SendDlgItemMessage(hDlgWnd, IDC_EDIT_FFPROBE_PATH, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(g_sFFprobePath.c_str()));
	SendDlgItemMessage(hDlgWnd, IDC_EDIT_FFMPEG_PATH, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(g_sFFmpegPath.c_str()));
}

bool UpdateValue(HWND hDlgWnd)
{
	g_nNumber = SendDlgItemMessage(hDlgWnd, IDC_SPIN_NUMBER, UDM_GETPOS32, 0, 0);
	g_fImages = (SendDlgItemMessage(hDlgWnd, IDC_RADIO_IMAGES, BM_GETCHECK, 0, 0) == BST_CHECKED);

	LRESULT lLen = SendDlgItemMessage(hDlgWnd, IDC_EDIT_FFPROBE_PATH, WM_GETTEXTLENGTH, 0, 0);
	std::vector<char> vBuf(lLen+1);
	SendDlgItemMessage(hDlgWnd, IDC_EDIT_FFPROBE_PATH, WM_GETTEXT, lLen+1, reinterpret_cast<LPARAM>(&vBuf[0]));
	g_sFFprobePath = std::string(&vBuf[0]);

	lLen = SendDlgItemMessage(hDlgWnd, IDC_EDIT_FFMPEG_PATH, WM_GETTEXTLENGTH, 0, 0);
	vBuf.resize(lLen+1);
	SendDlgItemMessage(hDlgWnd, IDC_EDIT_FFMPEG_PATH, WM_GETTEXT, lLen+1, reinterpret_cast<LPARAM>(&vBuf[0]));
	g_sFFmpegPath = std::string(&vBuf[0]);

	return true; // TODO: Always update
}

static void BrowseExePath(HWND hDlgWnd, bool fProbe)
{
	std::vector<char> buf(2048);
	int nFile, nExtension;
	if(fProbe) {
		std::copy(g_sFFprobePath.begin(), g_sFFprobePath.end(), buf.begin());
		buf[g_sFFprobePath.size()] = 0;
		nFile = g_sFFprobePath.find_last_of('\\');
		nExtension = g_sFFprobePath.find_last_of('.');
	} else {
		std::copy(g_sFFmpegPath.begin(), g_sFFmpegPath.end(), buf.begin());
		buf[g_sFFmpegPath.size()] = 0;
		nFile = g_sFFmpegPath.find_last_of('\\');
		nExtension = g_sFFmpegPath.find_last_of('.');
	}
	if(nFile < 0) nFile = 0;
	if(nExtension < 0) nExtension = 0;
	OPENFILENAME ofn = {
		sizeof(OPENFILENAME),
		hDlgWnd,
		0,
		fProbe ? "ffprobe.exe\0ffprobe.exe\0\0" : "ffmpeg.exe\0ffmpeg.exe\0\0",
		0,
		0,
		1,
		&buf[0],
		buf.size(),
		0,
		0,
		0,
		fProbe ? "Specify the place of ffprobe.exe" : "Specify the place of ffmpeg.exe",
		OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_LONGNAMES | OFN_PATHMUSTEXIST,
		nFile,
		nExtension,
		0,
		0,
		0,
		0
	};
	if(GetOpenFileName(&ofn)) {
		SendDlgItemMessage(hDlgWnd, fProbe ? IDC_EDIT_FFPROBE_PATH : IDC_EDIT_FFMPEG_PATH, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(&buf[0]));
	}
}

static LRESULT CALLBACK ConfigDlgProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
		case WM_INITDIALOG:
			UpdateDialogItem(hDlgWnd);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wp)) {
				case IDOK:
					if (UpdateValue(hDlgWnd)) {
						SaveToIni();
						EndDialog(hDlgWnd, IDOK);
					}
					break;
				case IDCANCEL:
					EndDialog(hDlgWnd, IDCANCEL);
					break;
				case IDC_BROWSE_FFPROBE:
				case IDC_BROWSE_FFMPEG:
					BrowseExePath(hDlgWnd, LOWORD(wp) == IDC_BROWSE_FFPROBE);
					break;
				default:
					return FALSE;
			}
		default:
			return FALSE;
	}
	return TRUE;
}

static HINSTANCE g_hInstance;

INT PASCAL ConfigurationDlg(HWND parent, INT fnc)
{
	DEBUG_LOG(<< "ConfigurationDlg called" << std::endl);
	if (fnc == 0) { // About
		INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_UPDOWN_CLASS };
		InitCommonControlsEx(&icex);
		DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_ABOUT_DIALOG), parent, (DLGPROC)AboutDlgProc);
	} else { // Configuration
		DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_CONFIG_DIALOG), parent, (DLGPROC)ConfigDlgProc, 0);
	}
	return 0;
}

extern "C" BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call) {
		case DLL_PROCESS_ATTACH:
			g_hInstance = (HINSTANCE)hModule;
			SetIniFileName(hModule);
			LoadFromIni();
			break;
	}
	return TRUE;
}
