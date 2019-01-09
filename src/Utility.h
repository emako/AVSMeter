/*
	This file is part of AVSMeter, Copyright(C) Groucho2004.

	AVSMeter is free software. You can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation, either
	version 3 of the License, or any later version.

	AVSMeter is distributed in the hope that it will be useful
	but WITHOUT ANY WARRANTY and without the implied warranty
	of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with AVSMeter. If not, see <http://www.gnu.org/licenses/>.
*/


#if !defined(_UTILITY_H)
#define _UTILITY_H

#include "common.h"

#define FG_BLACK    0
#define FG_RED			FOREGROUND_RED
#define FG_GREEN		FOREGROUND_GREEN
#define FG_BLUE			FOREGROUND_BLUE
#define FG_YELLOW		FOREGROUND_RED | FOREGROUND_GREEN
#define FG_MAGENTA	FOREGROUND_RED | FOREGROUND_BLUE
#define FG_CYAN			FOREGROUND_GREEN | FOREGROUND_BLUE
#define FG_HRED			FOREGROUND_RED | FOREGROUND_INTENSITY
#define FG_HGREEN		FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define FG_HBLUE		FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define FG_HYELLOW	FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY
#define FG_HMAGENTA	FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define FG_HCYAN		FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define FG_DARKGREY	FOREGROUND_INTENSITY
#define FG_GREY			FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED
#define FG_WHITE		FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY

#define BG_BLACK    0
#define BG_RED			BACKGROUND_RED
#define BG_GREEN		BACKGROUND_GREEN
#define BG_BLUE			BACKGROUND_BLUE
#define BG_YELLOW		BACKGROUND_RED | BACKGROUND_GREEN
#define BG_MAGENTA	BACKGROUND_RED | BACKGROUND_BLUE
#define BG_CYAN			BACKGROUND_GREEN | BACKGROUND_BLUE
#define BG_HRED			BACKGROUND_RED | BACKGROUND_INTENSITY
#define BG_HGREEN		BACKGROUND_GREEN | BACKGROUND_INTENSITY
#define BG_HBLUE		BACKGROUND_BLUE | BACKGROUND_INTENSITY
#define BG_HYELLOW	BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY
#define BG_HMAGENTA	BACKGROUND_RED | BACKGROUND_BLUE | BACKGROUND_INTENSITY
#define BG_HCYAN		BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY
#define BG_DARKGREY	BACKGROUND_INTENSITY
#define BG_GREY			BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_RED
#define BG_WHITE		BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_RED | BACKGROUND_INTENSITY


class CUtils
{
public:
	CUtils();
	virtual        ~CUtils();

	//File/directory/system
	string         SysErrorMessage();
	__int64        FileSize(string s_file);
	string         GetFileVersion(string s_file);
	string         GetProductVersion(string s_file);
	string         GetFileTimeStamp(string s_file);
	string         GetFileDateStamp(string s_file);
	BOOL           FileExists(string s_file);
	BOOL           DirectoryExists(string s_dir);

	//Console functions
	int            GetConsoleWidth();
	void           SetConsoleColors(WORD wAttributes);
	void           ResetConsoleColors();
	void           CursorUp(unsigned int iLines);

	//string functions
	string         StrFormat(char const *fmt, ...);
	void           StrTrimLeft(string &s_string);
	void           StrTrimRight(string &s_string);
	void           StrTrim(string &s_string);
	void           StrToUC(string &s_string);
	void           StrToLC(string &s_string);
	string         StrFormatFPS(double d_fps);
	string         StrFormatTPF(double d_frametime);
	string         StrAnsiToOEM(string s_string);
	void           StrTokenize(string s_string, vector<string> &tokens, string s_delimiter, BOOL b_removedups);
	BOOL           IsNumeric(string s_string);

private:
	HANDLE         hConsole;
	WORD           wSavedAttributes;
};


CUtils::CUtils()
{
	hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);

	//save attributes
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole, &csbi);
	wSavedAttributes = csbi.wAttributes;
}

CUtils::~CUtils()
{
	//restore attributes
	SetConsoleTextAttribute(hConsole, wSavedAttributes);
}


string CUtils::SysErrorMessage()
{
	DWORD dwLastError = GetLastError();
	if (dwLastError == ERROR_SUCCESS)
		return "";

	char msgBuf[2048];
	memset(msgBuf, 0, sizeof(msgBuf));
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwLastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msgBuf, 2046, NULL);

	string sRet(msgBuf);
	sRet.erase(sRet.find_last_not_of(" \t\n\r") + 1);
	sRet.erase(0, sRet.find_first_not_of(" \t\n\r"));

	return sRet;
}


int CUtils::GetConsoleWidth()
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole, &csbi);

	return (int)csbi.dwSize.X;
}


void CUtils::CursorUp(unsigned int iRows)
{
	COORD pos;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	::GetConsoleScreenBufferInfo(hConsole, &csbi);
	pos.Y = (short)(csbi.dwCursorPosition.Y - iRows);
	pos.X = 0;
	::SetConsoleCursorPosition(hConsole, pos);

	return;
}


void CUtils::SetConsoleColors(WORD wAttributes)
{
	SetConsoleTextAttribute(hConsole, wAttributes);
	return;
}


void CUtils::ResetConsoleColors()
{
	SetConsoleTextAttribute(hConsole, wSavedAttributes);
	return;
}


__int64 CUtils::FileSize(string s_file)
{
	WIN32_FIND_DATA fd;
	__int64 iSize = -1;
	HANDLE hFind = FindFirstFile(s_file.c_str(), &fd);
	if (hFind != INVALID_HANDLE_VALUE)
		iSize =	(((__int64)fd.nFileSizeHigh) << 32) + fd.nFileSizeLow;

	FindClose(hFind);

	return iSize;
}


BOOL CUtils::FileExists(string s_file)
{
	DWORD dwAttrib;
	dwAttrib = ::GetFileAttributes(s_file.c_str());

	if (dwAttrib == 0xFFFFFFFF)
		return FALSE;
	if (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)
		return FALSE;

	return TRUE;
}


BOOL CUtils::DirectoryExists(string s_dir)
{
	DWORD dwAttrib;
	dwAttrib = ::GetFileAttributes(s_dir.c_str());
	if (dwAttrib == 0xFFFFFFFF)
		return FALSE;
	if (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)
		return TRUE;

	return FALSE;
}


string CUtils::GetFileTimeStamp(string s_file)
{
	string sTimeStamp = "";
	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(s_file.c_str(), &fd);

	if (hFind == INVALID_HANDLE_VALUE)
		return "Cannot determine timestamp";

	SYSTEMTIME fTime;

	if (!FileTimeToSystemTime(&fd.ftLastWriteTime, &fTime))
	{
		FindClose(hFind);
		return "Cannot determine timestamp";
	}

	FindClose(hFind);

	sTimeStamp = StrFormat("%04u-%02u-%02u, %02u:%02u:%02u (UTC)", fTime.wYear, fTime.wMonth, fTime.wDay, fTime.wHour, fTime.wMinute, fTime.wSecond);

	return sTimeStamp;
}


string CUtils::GetFileDateStamp(string s_file)
{
	string sDateStamp = "";
	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(s_file.c_str(), &fd);

	if (hFind == INVALID_HANDLE_VALUE)
		return "Cannot determine timestamp";

	SYSTEMTIME fTime;

	if (!FileTimeToSystemTime(&fd.ftLastWriteTime, &fTime))
	{
		FindClose(hFind);
		return "Cannot determine timestamp";
	}

	FindClose(hFind);

	sDateStamp = StrFormat("%04u-%02u-%02u", fTime.wYear, fTime.wMonth, fTime.wDay);

	return sDateStamp;
}


string CUtils::StrFormat(char const *fmt, ...)
{
	#define MIN_BUF_SIZE   256
	#define MAX_BUF_SIZE 16384
	va_list args;
	va_start(args, fmt);
	vector<char> buffer(MIN_BUF_SIZE);
	int iRet = -1;

	for (;;)
	{
		iRet = _vsnprintf(&buffer[0], buffer.size() - 1, fmt, args);
		if (iRet == -1)
			buffer.resize(buffer.size() * 2);
		else
			break;

		if (buffer.size() > MAX_BUF_SIZE)
		{
			va_end(args);
			return "buffer overflow";
		}
	}

	buffer[buffer.size() - 1] = '\0';
	string sRet = &buffer[0];
	va_end(args);

	return sRet;
}


void CUtils::StrTrimLeft(string &s_string)
{
	s_string.erase(0, s_string.find_first_not_of(" \t\n\r"));
	return;
}


void CUtils::StrTrimRight(string &s_string)
{
	s_string.erase(s_string.find_last_not_of(" \t\n\r") + 1);
	return;
}


void CUtils::StrTrim(string &s_string)
{
	s_string.erase(s_string.find_last_not_of(" \t\n\r") + 1);
	s_string.erase(0, s_string.find_first_not_of(" \t\n\r"));
	return;
}


void CUtils::StrToUC(string &s_string)
{
	transform(s_string.begin(), s_string.end(), s_string.begin(), ::toupper);
	return;
}


void CUtils::StrToLC(string &s_string)
{
	transform(s_string.begin(), s_string.end(), s_string.begin(), ::tolower);
	return;
}


string CUtils::StrFormatFPS(double d_fps)
{
	string sRet = "";

	if (d_fps < 9.999)      sRet = StrFormat("%.3f", d_fps);
	else if (d_fps < 99.99) sRet = StrFormat("%.2f", d_fps);
	else if (d_fps < 999.9) sRet = StrFormat("%.1f", d_fps);
	else                    sRet = StrFormat("%u", (unsigned int)(d_fps + 0.5));

	return sRet;
}


string CUtils::StrFormatTPF(double d_frametime)
{
	string sRet = "";

	if (d_frametime < 0.1)         sRet = StrFormat("%.5f", d_frametime);
	else if (d_frametime < 1.0)    sRet = StrFormat("%.4f", d_frametime);
	else if (d_frametime < 10.0)   sRet = StrFormat("%.3f", d_frametime);
	else if (d_frametime < 100.0)  sRet = StrFormat("%.2f", d_frametime);
	else if (d_frametime < 1000.0) sRet = StrFormat("%.1f", d_frametime);
	else                           sRet = StrFormat("%u", (unsigned int)(d_frametime + 0.5));

	return sRet;
}


string CUtils::GetFileVersion(string s_file)
{
	string sFileVersion = "n/a";
	vector<char> szFileName(MAX_PATH + 1);

	if (s_file == "")
	{
		if (GetModuleFileName(NULL, &szFileName[0], (DWORD)szFileName.size() - 1) == 0)
			return sFileVersion;
	}
	else
	{
		if (_snprintf(&szFileName[0], szFileName.size() - 1, "%s", s_file.c_str()) < 0)
			return sFileVersion;
	}

	DWORD dummy;
	DWORD dwSize = GetFileVersionInfoSize(&szFileName[0], &dummy);
	if (dwSize == 0)
		return sFileVersion;

	BYTE* VersionInfoBuffer = new BYTE[dwSize];
	if (!GetFileVersionInfo(&szFileName[0], 0, dwSize, (VOID*)VersionInfoBuffer))
	{
		delete [] VersionInfoBuffer;
		return sFileVersion;
	}

	UINT uiLen;
	VS_FIXEDFILEINFO* pFixedInfo = 0;
	if (!VerQueryValue(VersionInfoBuffer, "\\", (LPVOID*)(&pFixedInfo), &uiLen))
	{
		delete [] VersionInfoBuffer;
		return sFileVersion;
	}

	sFileVersion = StrFormat("%u.%u.%u.%u", HIWORD(pFixedInfo->dwFileVersionMS), LOWORD(pFixedInfo->dwFileVersionMS), HIWORD(pFixedInfo->dwFileVersionLS), LOWORD(pFixedInfo->dwFileVersionLS));
	delete [] VersionInfoBuffer;

	return sFileVersion;
}


string CUtils::GetProductVersion(string s_file)
{
	string sProductVersion = "n/a";
	vector<char> szFileName(MAX_PATH + 1);

	if (s_file == "")
	{
		if (GetModuleFileName(NULL, &szFileName[0], (DWORD)szFileName.size() - 1) == 0)
			return sProductVersion;
	}
	else
	{
		if (_snprintf(&szFileName[0], szFileName.size() - 1, "%s", s_file.c_str()) < 0)
			return sProductVersion;
	}

	DWORD dummy;
	DWORD dwSize = GetFileVersionInfoSize(&szFileName[0], &dummy);
	if (dwSize == 0)
		return sProductVersion;

	BYTE* VersionInfoBuffer = new BYTE[dwSize];
	if (!GetFileVersionInfo(&szFileName[0], 0, dwSize, (VOID*)VersionInfoBuffer))
	{
		delete [] VersionInfoBuffer;
		return sProductVersion;
	}

	UINT uiLen;
	VS_FIXEDFILEINFO* pFixedInfo = 0;
	if (!VerQueryValue(VersionInfoBuffer, "\\", (LPVOID*)(&pFixedInfo), &uiLen))
	{
		delete [] VersionInfoBuffer;
		return sProductVersion;
	}

	sProductVersion = StrFormat("%u.%u.%u.%u", HIWORD(pFixedInfo->dwProductVersionMS), LOWORD(pFixedInfo->dwProductVersionMS), HIWORD(pFixedInfo->dwProductVersionLS), LOWORD(pFixedInfo->dwProductVersionLS));
	delete [] VersionInfoBuffer;

	return sProductVersion;
}


BOOL CUtils::IsNumeric(string s_string)
{
	if (s_string == "")
		return FALSE;

	for (unsigned int ipos = 0; ipos < s_string.length(); ipos++)
	{
		switch (s_string[ipos])
		{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '-':
			//case '.':
				continue;
			default:
				return FALSE;
		}
	}

	return TRUE;
}


void CUtils::StrTokenize(string s_string, vector<string> &tokens, string s_delimiter, BOOL b_removedups)
{
	StrTrim(s_string);
	string sToken = "";
	string sTokenLC = "";
	set <string> mDuplicates;

	size_t current = 0;
	size_t next = -1;
	do
	{
		current = next + 1;
		next = s_string.find_first_of(s_delimiter, current);
		sToken = s_string.substr(current, next - current);
		StrTrim(sToken);
		sTokenLC = sToken;
		StrToLC(sTokenLC);
		if ((sToken != "") && (mDuplicates.find(sTokenLC) == mDuplicates.end()))
		{
			tokens.push_back(sToken);
			if (b_removedups)
				mDuplicates.insert(sTokenLC);
		}
	}
	while (next != string::npos);

	return;
}


string CUtils::StrAnsiToOEM(string s_string)
{
	vector<char> buffer(s_string.length() + 1);
	CharToOem(s_string.c_str(), &buffer[0]);
	string sOEM(&buffer[0]);
	return sOEM;
}


#endif //_UTILITY_H
