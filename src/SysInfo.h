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


#if !defined(_SYSINFO_H)
#define _SYSINFO_H

#include "common.h"
#include "utility.h"

#include <immintrin.h>
#include <intrin.h>

class CSysInfo
{
public:
	CSysInfo();
	virtual           ~CSysInfo();

	string            GetOSVersion();
	string            GetFormattedSystemDateTime();
	BOOL              Is64BitOS();
	BOOL              GetCPUInfo();
	string            CPUBrandString;
	string            CPUVendorString;
	string            CPUFeatures;

private:
	CUtils            utils;
	void              cpuid(int *regs, int info_type);
	unsigned __int64  xgetbv (int ctr);
	void              GetCPUFeatures();
	string            RemoveMultipleWhiteSpaces(string s);
};


CSysInfo::CSysInfo()
{
}

CSysInfo::~CSysInfo()
{
}


string CSysInfo::GetOSVersion()
{
	typedef LONG NTSTATUS, *PNTSTATUS;
	typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOEXW);
	string sOSVersion = "Unknown OS Version";
	string sOSAddendum = "";

	string sArchitecture = "(x86)";
	if (Is64BitOS())
		sArchitecture = "(x64)";

	HMODULE hMod = ::GetModuleHandle("ntdll.dll");
	if (!hMod)
		return "Cannot get module handle to ntdll.dll";

	RtlGetVersionPtr rgvPtr = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
	if (rgvPtr == NULL)
		return "Cannot invoke \"RtlGetVersion\"";

	RTL_OSVERSIONINFOEXW osvi = {0};
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	if (rgvPtr(&osvi) != 0)
		return "Cannot invoke \"RtlGetVersion\"";

	if (osvi.dwMajorVersion == 5)
	{
		if (osvi.wProductType == VER_NT_WORKSTATION)
			sOSVersion = utils.StrFormat("Windows XP %s", sArchitecture.c_str());
		else
			sOSVersion = utils.StrFormat("Windows Server 2003 %s", sArchitecture.c_str());
	}

	if (osvi.dwMajorVersion == 6)
	{
		if (osvi.dwMinorVersion == 0)
		{
			if (osvi.wProductType == VER_NT_WORKSTATION)
				sOSVersion = utils.StrFormat("Windows Vista %s", sArchitecture.c_str());
			else
				sOSVersion = utils.StrFormat("Windows Server 2008 %s", sArchitecture.c_str());
		}
		if (osvi.dwMinorVersion == 1)
		{
			if (osvi.wProductType == VER_NT_WORKSTATION)
				sOSVersion = utils.StrFormat("Windows 7 %s", sArchitecture.c_str());
			else
				sOSVersion = utils.StrFormat("Windows Server 2008 R2 %s", sArchitecture.c_str());
		}
		if (osvi.dwMinorVersion == 2)
		{
			if (osvi.wProductType == VER_NT_WORKSTATION)
				sOSVersion = utils.StrFormat("Windows 8 %s", sArchitecture.c_str());
			else
				sOSVersion = utils.StrFormat("Windows Server 2012 %s", sArchitecture.c_str());
		}
		if (osvi.dwMinorVersion == 3)
		{
			if (osvi.wProductType == VER_NT_WORKSTATION)
				sOSVersion = utils.StrFormat("Windows 8.1 %s", sArchitecture.c_str());
			else
				sOSVersion = utils.StrFormat("Windows Server 2012 R2 %s", sArchitecture.c_str());
		}
	}

	if (osvi.dwMajorVersion == 10)
	{
		if (osvi.dwMinorVersion == 0)
			sOSVersion = utils.StrFormat("Windows 10 %s", sArchitecture.c_str());
	}

	if ((osvi.wServicePackMajor > 0) || (osvi.wServicePackMinor > 0))
		sOSAddendum = utils.StrFormat(" Service Pack %u.%u (Build %u)", osvi.wServicePackMajor, osvi.wServicePackMinor, osvi.dwBuildNumber);
	else
		sOSAddendum = utils.StrFormat(" (Build %u)", osvi.dwBuildNumber);

	return sOSVersion + sOSAddendum;
}


string CSysInfo::GetFormattedSystemDateTime()
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	return utils.StrFormat("%04d%02d%02d-%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}


BOOL CSysInfo::GetCPUInfo()
{
	CPUBrandString = "";
	char szCPUBrandString[128];
	unsigned nIDs, i;
	int CPURegs[4];

	cpuid(CPURegs, 0x80000000);
	nIDs = CPURegs[0];

	if (nIDs < 0x80000004)
		return FALSE;

	memset(szCPUBrandString, 0, sizeof(szCPUBrandString));

	for (i = 0x80000000; i <= nIDs; ++i)
	{
		cpuid(CPURegs, i);

		if (i == 0x80000002) memcpy(szCPUBrandString, CPURegs, sizeof(CPURegs));
		if (i == 0x80000003) memcpy(szCPUBrandString + 16, CPURegs, sizeof(CPURegs));
		if (i == 0x80000004) memcpy(szCPUBrandString + 32, CPURegs, sizeof(CPURegs));
	}

	CPUBrandString = utils.StrFormat("%s", szCPUBrandString);
	CPUBrandString.erase(CPUBrandString.find_last_not_of(" \t\n\r") + 1);
	CPUBrandString.erase(0, CPUBrandString.find_first_not_of(" \t\n\r"));
	CPUBrandString = RemoveMultipleWhiteSpaces(CPUBrandString);

	GetCPUFeatures();

	return TRUE;
}


void CSysInfo::GetCPUFeatures()
{
	BOOL CPU_FEATURE_MMX =        FALSE;
	BOOL CPU_FEATURE_SSE =        FALSE;
	BOOL CPU_FEATURE_SSE2 =       FALSE;
	BOOL CPU_FEATURE_SSE3 =       FALSE;
	BOOL CPU_FEATURE_SSSE3 =      FALSE;
	BOOL CPU_FEATURE_SSE41 =      FALSE;
	BOOL CPU_FEATURE_SSE42 =      FALSE;
	BOOL CPU_FEATURE_SSE4A =      FALSE;
	BOOL CPU_FEATURE_AVX =        FALSE;
	BOOL CPU_FEATURE_AVX2 =       FALSE;
	BOOL CPU_FEATURE_AVX512F =    FALSE;
	BOOL CPU_FEATURE_AVX512VL =   FALSE;
	BOOL CPU_FEATURE_AVX512BW =   FALSE;
	BOOL CPU_FEATURE_AVX512DQ =   FALSE;
	BOOL CPU_FEATURE_AVX512ER =   FALSE;
	BOOL CPU_FEATURE_AVX512CD =   FALSE;
	BOOL CPU_FEATURE_AVX512PF =   FALSE;
	BOOL CPU_FEATURE_AVX512IFMA = FALSE;
	BOOL CPU_FEATURE_AVX512VBMI = FALSE;
	BOOL CPU_FEATURE_3DNOW =      FALSE;
	BOOL CPU_FEATURE_3DNOWEXT =   FALSE;
	BOOL CPU_FEATURE_MMXEXT =     FALSE;
	BOOL CPU_FEATURE_FMA3 =       FALSE;
	BOOL CPU_FEATURE_FMA4 =       FALSE;
	BOOL CPU_FEATURE_F16C =       FALSE;
	BOOL CPU_FEATURE_MOVBE =      FALSE;
	BOOL CPU_FEATURE_POPCNT =     FALSE;
	BOOL CPU_FEATURE_AES =        FALSE;

	int CPURegs[4];
	cpuid(CPURegs, 0);
	int nIds = CPURegs[0];

	cpuid(CPURegs, 0x80000000);
	unsigned nExIds = CPURegs[0];

	if (nIds >= 0x00000001)
	{
		cpuid(CPURegs, 0x00000001);
		CPU_FEATURE_MMX    = (CPURegs[3] & (1 << 23)) != 0;
		CPU_FEATURE_SSE    = (CPURegs[3] & (1 << 25)) != 0;
		CPU_FEATURE_SSE2   = (CPURegs[3] & (1 << 26)) != 0;
		CPU_FEATURE_SSE3   = (CPURegs[2] & (1 <<  0)) != 0;
		CPU_FEATURE_SSSE3  = (CPURegs[2] & (1 <<  9)) != 0;
		CPU_FEATURE_SSE41  = (CPURegs[2] & (1 << 19)) != 0;
		CPU_FEATURE_SSE42  = (CPURegs[2] & (1 << 20)) != 0;
		CPU_FEATURE_AVX    = (CPURegs[2] & (1 << 28)) != 0;
		CPU_FEATURE_FMA3   = (CPURegs[2] & (1 << 12)) != 0;
		CPU_FEATURE_MOVBE  = (CPURegs[2] & (1 << 22)) != 0;
		CPU_FEATURE_POPCNT = (CPURegs[2] & (1 << 23)) != 0;
		CPU_FEATURE_AES    = (CPURegs[2] & (1 << 25)) != 0;
		CPU_FEATURE_F16C   = (CPURegs[2] & (1 << 29)) != 0;
	}

	if (nIds >= 0x00000007)
	{
		cpuid(CPURegs, 0x00000007);
		CPU_FEATURE_AVX2        = (CPURegs[1] & (1 <<  5)) != 0;
		CPU_FEATURE_AVX512F     = (CPURegs[1] & (1 << 16)) != 0;
		CPU_FEATURE_AVX512CD    = (CPURegs[1] & (1 << 28)) != 0;
		CPU_FEATURE_AVX512PF    = (CPURegs[1] & (1 << 26)) != 0;
		CPU_FEATURE_AVX512ER    = (CPURegs[1] & (1 << 27)) != 0;
		CPU_FEATURE_AVX512VL    = (CPURegs[1] & (1 << 31)) != 0;
		CPU_FEATURE_AVX512BW    = (CPURegs[1] & (1 << 30)) != 0;
		CPU_FEATURE_AVX512DQ    = (CPURegs[1] & (1 << 17)) != 0;
		CPU_FEATURE_AVX512IFMA  = (CPURegs[1] & (1 << 21)) != 0;
		CPU_FEATURE_AVX512VBMI  = (CPURegs[2] & (1 <<  1)) != 0;
	}

	if (nExIds >= 0x80000001)
	{
		cpuid(CPURegs, 0x80000001);
		CPU_FEATURE_3DNOW =    (CPURegs[3] & (1 << 31)) != 0;
		CPU_FEATURE_3DNOWEXT = (CPURegs[3] & (1 << 30)) != 0;
		CPU_FEATURE_MMXEXT =   (CPURegs[3] & (1 << 22)) != 0;
		CPU_FEATURE_SSE4A =    (CPURegs[2] & (1 <<  6)) != 0;
		CPU_FEATURE_FMA4  =    (CPURegs[2] & (1 << 16)) != 0;
	}


	BOOL OS_FEATURE_AVX =        FALSE;
	BOOL OS_FEATURE_AVX512 =     FALSE;

	#ifndef _XCR_XFEATURE_ENABLED_MASK
	#define _XCR_XFEATURE_ENABLED_MASK 0
	#endif

	unsigned __int64 xcrFeatureMask = 0;
	cpuid(CPURegs, 1);
	BOOL XSAVE = (CPURegs[2] & (1 << 27)) != 0;

	if (XSAVE)
		xcrFeatureMask = xgetbv(_XCR_XFEATURE_ENABLED_MASK);

	if ((xcrFeatureMask & 0x6) == 0x6)
		OS_FEATURE_AVX = TRUE;

	if ((xcrFeatureMask & (0x7 << 5)) && (xcrFeatureMask & (0x3 << 1)))
		OS_FEATURE_AVX512 = OS_FEATURE_AVX;

	CPUFeatures = "";

	if (CPU_FEATURE_MMX)        CPUFeatures += "MMX, ";
	if (CPU_FEATURE_SSE)        CPUFeatures += "SSE, ";
	if (CPU_FEATURE_SSE2)       CPUFeatures += "SSE2, ";
	if (CPU_FEATURE_SSE3)       CPUFeatures += "SSE3, ";
	if (CPU_FEATURE_SSSE3)      CPUFeatures += "SSSE3, ";
	if (CPU_FEATURE_SSE41)      CPUFeatures += "SSE4.1, ";
	if (CPU_FEATURE_SSE42)      CPUFeatures += "SSE4.2, ";

	if (CPU_FEATURE_AVX)
	{
		if (!OS_FEATURE_AVX) CPUFeatures += "*";
		CPUFeatures += "AVX, ";
	}

	if (CPU_FEATURE_AVX2)
	{
		if (!OS_FEATURE_AVX) CPUFeatures += "*";
		CPUFeatures += "AVX2, ";
	}

	if (CPU_FEATURE_FMA3)
	{
		if (!OS_FEATURE_AVX) CPUFeatures += "*";
		CPUFeatures += "FMA3, ";
	}

	if (CPU_FEATURE_FMA4)
	{
		if (!OS_FEATURE_AVX) CPUFeatures += "*";
		CPUFeatures += "FMA4, ";
	}

	if (CPU_FEATURE_AVX512F)
	{
		if (!OS_FEATURE_AVX512) CPUFeatures += "*";
		CPUFeatures += "AVX512F, ";
	}

	if (CPU_FEATURE_AVX512DQ)
	{
		if (!OS_FEATURE_AVX512) CPUFeatures += "*";
		CPUFeatures += "AVX512DQ, ";
	}

	if (CPU_FEATURE_AVX512PF)
	{
		if (!OS_FEATURE_AVX512) CPUFeatures += "*";
		CPUFeatures += "AVX512PF, ";
	}

	if (CPU_FEATURE_AVX512ER)
	{
		if (!OS_FEATURE_AVX512) CPUFeatures += "*";
		CPUFeatures += "AVX512ER, ";
	}

	if (CPU_FEATURE_AVX512CD)
	{
		if (!OS_FEATURE_AVX512) CPUFeatures += "*";
		CPUFeatures += "AVX512CD, ";
	}

	if (CPU_FEATURE_AVX512BW)
	{
		if (!OS_FEATURE_AVX512) CPUFeatures += "*";
		CPUFeatures += "AVX512BW, ";
	}

	if (CPU_FEATURE_AVX512VL)
	{
		if (!OS_FEATURE_AVX512) CPUFeatures += "*";
		CPUFeatures += "AVX512VL, ";
	}

	if (CPU_FEATURE_AVX512IFMA)
	{
		if (!OS_FEATURE_AVX512) CPUFeatures += "*";
		CPUFeatures += "AVX512IFMA, ";
	}

	if (CPU_FEATURE_AVX512VBMI)
	{
		if (!OS_FEATURE_AVX512) CPUFeatures += "*";
		CPUFeatures += "AVX512VBMI, ";
	}

	if (CPU_FEATURE_SSE4A)      CPUFeatures += "SSE4A, ";
	if (CPU_FEATURE_3DNOW)      CPUFeatures += "3DNOW, ";
	if (CPU_FEATURE_3DNOWEXT)   CPUFeatures += "3DNOWEXT, ";
	if (CPU_FEATURE_MMXEXT)     CPUFeatures += "MMXEXT, ";
	if (CPU_FEATURE_MOVBE)      CPUFeatures += "MOVBE, ";
	if (CPU_FEATURE_POPCNT)     CPUFeatures += "POPCNT, ";
	if (CPU_FEATURE_AES)        CPUFeatures += "AES, ";
	if (CPU_FEATURE_F16C)       CPUFeatures += "F16C, ";

	CPUFeatures.erase(CPUFeatures.find_last_not_of(", ") + 1);

	return;
}


void CSysInfo::cpuid(int *regs, int info_type)
{
	regs[0] = 0;
	regs[1] = 0;
	regs[2] = 0;
	regs[3] = 0;

	#if (_MSC_VER < 1500)
	__asm
	{
		mov   esi, regs
		mov   eax, info_type
		xor   ecx, ecx
		cpuid
		mov   dword ptr [esi +  0], eax
		mov   dword ptr [esi +  4], ebx
		mov   dword ptr [esi +  8], ecx
		mov   dword ptr [esi + 12], edx
  }
	#else
		__cpuid(regs, info_type);
	#endif

	return;
}


unsigned __int64 CSysInfo::xgetbv(int ctr)
{
	#if (defined (_MSC_FULL_VER) && _MSC_FULL_VER >= 160040219)
		return _xgetbv(ctr);
	#else
		unsigned int a, d;
		__asm
		{
			mov ecx, ctr
			_emit 0x0F
			_emit 0x01
			_emit 0xD0
			mov a, eax
			mov d, edx
		}
		return a | ((unsigned __int64)d << 32);
	#endif
}


string CSysInfo::RemoveMultipleWhiteSpaces(string s)
{
	string search = "  ";
	size_t index;
	while((index = s.find(search)) != string::npos) s.erase(index,1);

	return s;
}


BOOL CSysInfo::Is64BitOS()
{
	if (sizeof(void*) == 8)
		return TRUE; //64 on 64

	BOOL bWoW64Process = FALSE;
	typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	LPFN_ISWOW64PROCESS fnIsWow64Process;
	HMODULE hKernel32 = GetModuleHandle("kernel32.dll");
	if (hKernel32 == NULL)
		return FALSE;

	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(hKernel32, "IsWow64Process");
	if (fnIsWow64Process != NULL)
		fnIsWow64Process(GetCurrentProcess(), &bWoW64Process);
	if (bWoW64Process)
		return TRUE;

	return FALSE;
}


#endif //_SYSINFO_H
