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


#if !defined(_COMMON_H)
#define _COMMON_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <winver.h>
#include <winnt.h>
#include <shlobj.h>
#include <commdlg.h>
#include <process.h>
#include <math.h>
#include <string>
#include <algorithm>
#include <mmsystem.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <wintrust.h>
#include <imagehlp.h>
#include <conio.h>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <eh.h>

#define sprintf sprintf_s
//#define _snprintf _snprintf_s

using std::string;
using std::ifstream;
using std::ofstream;
using std::vector;
using std::sort;
using std::map;
using std::set;
using std::transform;
using std::getline;
using std::remove;
using std::exception;

#define DSE_DELAY 500
const BOOL PROCESS_64 = (sizeof(void*) == 8) ? TRUE : FALSE;
const HWND ConsoleHWND = GetConsoleWindow();

static BOOL CompareNoCase(string first, string second)
{
  size_t i = 0;
  while ((i < first.length()) && (i < second.length()))
  {
    if (tolower (first[i]) < tolower (second[i]))
			return true;
    else if (tolower (first[i]) > tolower (second[i]))
			return false;
 
   i++;
  }

  if (first.length() < second.length())
		return true;
  else
		return false;
}

string GetWOW64FilePath(string s_filepath);

string GetWOW64FilePath(string s_filepath)
{
	string sRealFilePath = s_filepath;

	BOOL bWoW64Process = FALSE;
	typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	LPFN_ISWOW64PROCESS fnIsWow64Process;

	HMODULE hKernel32 = GetModuleHandle("kernel32.dll");
	if (hKernel32 == NULL) return sRealFilePath;

	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(hKernel32, "IsWow64Process");
	if (fnIsWow64Process != NULL)
		fnIsWow64Process(GetCurrentProcess(), &bWoW64Process);
	else
		return sRealFilePath;

	if (bWoW64Process)
	{
		char pszWinDir[1026];
		GetWindowsDirectory(pszWinDir, 1024);
		string sTestSystem32(pszWinDir);
		sTestSystem32 = sTestSystem32 + "\\system32\\";
		transform(sTestSystem32.begin(), sTestSystem32.end(), sTestSystem32.begin(), ::tolower);
		string sTempDLLPath = s_filepath;
		transform(sTempDLLPath.begin(), sTempDLLPath.end(), sTempDLLPath.begin(), ::tolower);
		size_t slen = sTestSystem32.length();
		if (sTempDLLPath.substr(0, slen) == sTestSystem32)
		{
			char szSysWOW64Directory[1026];
			typedef UINT (WINAPI *LPFN_GETSYSTEMWOW64DIRECTORY)(LPTSTR, UINT);
			LPFN_GETSYSTEMWOW64DIRECTORY getSystemWow64Directory;

			getSystemWow64Directory = (LPFN_GETSYSTEMWOW64DIRECTORY)GetProcAddress(hKernel32, "GetSystemWow64DirectoryA");
			if (getSystemWow64Directory == NULL) return sRealFilePath;

			if (getSystemWow64Directory(szSysWOW64Directory, sizeof(szSysWOW64Directory)) > 1)
			{
				string sSysWOW64Dir(szSysWOW64Directory);
				sRealFilePath = sSysWOW64Dir + "\\" + s_filepath.substr(slen);
			}
		}
	}

	return sRealFilePath;
}


#endif //_COMMON_H
