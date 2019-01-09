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


#if !defined(_AVISYNTHINFO_H)
#define _AVISYNTHINFO_H

#include "common.h"
#include "exception.h"
#include "utility.h"
#include "avs_headers\avisynth.h"

const AVS_Linkage *AVS_linkage = 0;

class CAvisynthInfo
{
public:
	CAvisynthInfo();
	virtual ~CAvisynthInfo();

	BOOL    GetInfo(string s_AVSDLL, BOOL b_SpecifyCustomPluginDir, string &s_ErrorMsg);
	int     iInterfaceVersion;
	vector  <string> vPluginDirs;
	vector  <string> vInternalFunctions;
	vector  <string> vPluginFunctions;
	vector  <string> vPlugins;
	vector  <string> vPluginDependencies;
	vector  <string> vPluginErrors;
	string  sDLLPath;
	string  sFileVersion;
	string  sProductVersion;
	string  sVersionString;
	string  sVersionNumber;
	string  sTimeStamp;
	BOOL    bIs64BitAVSDLL;
	BOOL    bIsAVSPlus;
	BOOL    bIsMTVersion;

private:
	CUtils              utils;
	string              sAVSDLL;
  void                EnumPluginDirs(BOOL b_CustomPluginDir);
	void                EnumPluginDLLs();
	BOOL                GetPluginType(string s_dll, string &s_Msg, BOOL &b_Is64BitDLL);
	void                TestLoadPlugins();
	void                GetDLLDependencies(string s_dll, string &s_dependencies, string &s_failed_dependencies, string &s_hint);
	BOOL                IsRuntimeInstalled(string s_version);
	__int64             FileSize(string s_file);
	BOOL                Is64BitOS();
	string              BrowseDirectory();
	typedef             IScriptEnvironment * __stdcall CREATE_ENV(int);
};


CAvisynthInfo::CAvisynthInfo()
{
}

CAvisynthInfo::~CAvisynthInfo()
{
}


string CAvisynthInfo::BrowseDirectory()
{
	BROWSEINFO		bi;
	LPITEMIDLIST	pidlBrowse;
	char					szFullDir [MAX_PATH + 1];
	string				sDir = "INVALID";

	bi.hwndOwner      = NULL;
	bi.pidlRoot       = NULL;
	bi.pszDisplayName = NULL;
	bi.lpszTitle      = "Specify custom plugin directory:";
	bi.ulFlags        = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;
	bi.lpfn           =	NULL;
	bi.lParam         = NULL;
	bi.iImage         = 0;

	pidlBrowse = SHBrowseForFolder (&bi);	
	if (pidlBrowse != NULL)
	{
		if (SHGetPathFromIDList(pidlBrowse, szFullDir))
		{
			sDir = utils.StrFormat("%s", szFullDir);
			utils.StrTrim(sDir);
			if ((!utils.DirectoryExists(sDir)) || (sDir.length() < 4))
				sDir = "INVALID";
		}
	}

	return sDir;
}


BOOL CAvisynthInfo::GetInfo(string s_AVSDLL, BOOL b_CustomPluginDir, string &s_ErrorMsg)
{
	sDLLPath = "";
	bIs64BitAVSDLL = FALSE;
	bIsMTVersion = FALSE;
	bIsAVSPlus = FALSE;
	iInterfaceVersion = 0;
	sFileVersion = "";
	sProductVersion = "";
	sVersionString = "Unknown Avisynth Version";
	BOOL bAVSLinkage = FALSE;
	BOOL bSuccess = TRUE;
	s_ErrorMsg = "";

	LOADED_IMAGE li;
	BOOL bLoaded;

	//Make a local copy
	sAVSDLL = s_AVSDLL;

	if (sAVSDLL == "")
		bLoaded = MapAndLoad("avisynth", NULL, &li,	TRUE,	TRUE);
	else
		bLoaded = MapAndLoad((LPSTR)sAVSDLL.c_str(), NULL, &li,	TRUE,	TRUE);

	if (!bLoaded)
	{
		s_ErrorMsg = utils.SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll:\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";

		return FALSE;
	}

	try
	{
		_set_se_translator(SE_Translator);
		sDLLPath = utils.StrFormat("%s", li.ModuleName);

		if (li.FileHeader->FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
		{
			bIs64BitAVSDLL = TRUE;
			if (!PROCESS_64) //trying to load 64 bit avisynth.dll with 32 bit AVSMeter
			{
				UnMapAndLoad(&li);
				s_ErrorMsg = "AVSMeter (x86) cannot load a 64 Bit avisynth.dll.";
				return FALSE;
			}
		}
		else //32 Bit Avisynth DLL
		{
			if (PROCESS_64) //trying to load 32 bit avisynth.dll with 64 bit AVSMeter
			{
				UnMapAndLoad(&li);
				s_ErrorMsg = "AVSMeter (x64) cannot load a 32 Bit avisynth.dll.";
				return FALSE;
			}
		}

		DWORD expVA = li.FileHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
		PIMAGE_EXPORT_DIRECTORY pExp = (PIMAGE_EXPORT_DIRECTORY)ImageRvaToVa(li.FileHeader, li.MappedAddress, expVA, NULL);
		DWORD rvaNames = pExp->AddressOfNames;
		DWORD *prvaNames = (DWORD*)ImageRvaToVa(li.FileHeader, li.MappedAddress, rvaNames, NULL);

		for (DWORD dwName = 0; dwName < pExp->NumberOfNames; ++dwName)
		{
			DWORD rvaName = prvaNames[dwName];
			string sName((char *)ImageRvaToVa(li.FileHeader, li.MappedAddress, rvaName, NULL));
			utils.StrToLC(sName);

			if (sName.find("avs_linkage") != string::npos)
			{
				bAVSLinkage = TRUE;
				break;
			}
		}
	}
	catch (exception& ex)
	{
		bSuccess = FALSE;
		s_ErrorMsg = ex.what();
	}
	catch (...)
	{
		bSuccess = FALSE;
		s_ErrorMsg = utils.SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll:\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";
	}

	UnMapAndLoad(&li);

	if (!bSuccess)
		return FALSE;


	if (!bAVSLinkage)
	{
		string sFileVersion = utils.GetFileVersion(sDLLPath);

		if (sFileVersion.substr(0, 3) == "2.6")
			s_ErrorMsg = utils.StrFormat("The installed Avisynth version is a 2.6 Alpha or RC:\n%s\n\nUpdate to Avisynth 2.6 Release or Avisynth+\n\n", GetWOW64FilePath(sDLLPath).c_str());
		else
			s_ErrorMsg = utils.StrFormat("Avisynth version is too old:\n%s (Version: %s)\n\nUpdate to Avisynth 2.6 or Avisynth+\n\n", GetWOW64FilePath(sDLLPath).c_str(), sFileVersion.c_str());

		return FALSE;
	}


	sDLLPath = GetWOW64FilePath(sDLLPath);
	sFileVersion = utils.GetFileVersion(sDLLPath);
	sProductVersion = utils.GetProductVersion(sDLLPath);
	sTimeStamp = utils.GetFileTimeStamp(sDLLPath);

	string sDeps = "";
	string sFailedDeps = "";
	string sNote = "";
	string sHint = "";
	GetDLLDependencies(sDLLPath, sDeps, sFailedDeps, sHint);
	if (sDeps != "")
		vPluginDependencies.push_back(sDeps);

	if (sFailedDeps != "")
	{
		s_ErrorMsg = utils.StrFormat("Cannot load avisynth.dll\n\nDependencies that could not be loaded:\n%s", sFailedDeps.c_str());
		if (sHint != "")
			s_ErrorMsg += utils.StrFormat("\n\nNote:\n  %s\n", sHint.c_str());

		return FALSE;
	}

	HINSTANCE hDLL;
	if (sAVSDLL == "")
		hDLL = ::LoadLibrary("avisynth");
	else
		hDLL = ::LoadLibrary(sAVSDLL.c_str());

	if (!hDLL)
	{
		s_ErrorMsg = utils.SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll:\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";

		return FALSE;
	}

	IScriptEnvironment *AVS_env = 0;

	try
	{
		_set_se_translator(SE_Translator);

		CREATE_ENV *CreateEnvironment = (CREATE_ENV *)GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			s_ErrorMsg = "Failed to load CreateScriptEnvironment()";
			::FreeLibrary(hDLL);
			return FALSE;
		}

		iInterfaceVersion = 6;
		while (!AVS_env)
		{
			if (iInterfaceVersion < 5)
			{
				s_ErrorMsg = "Cannot create IScriptenvironment";
				::FreeLibrary(hDLL);
				return 0;
			}

			AVS_env = CreateEnvironment(iInterfaceVersion);
			iInterfaceVersion--;
		}


		AVS_linkage = AVS_env->GetAVSLinkage();
		AVSValue foo;

		if (AVS_env->FunctionExists("AutoloadPlugins"))
			bIsAVSPlus = TRUE;
		if ((AVS_env->FunctionExists("SetMTMode")) || (AVS_env->FunctionExists("Prefetch")))
			bIsMTVersion = TRUE;

		try
		{
			string sTemp = utils.StrFormat("%f", AVS_env->Invoke("VersionNumber", AVSValue(&foo, 0)).AsFloat());
			size_t x;
			for (x = sTemp.length() - 1; x > 3; x--)
			{
				if (sTemp[x] != '0')
					break;
			}
			sVersionNumber = sTemp.substr(0, x + 1);
		}
		catch (IScriptEnvironment::NotFound)
		{
			sVersionNumber = "Cannot invoke \"VersionNumber\"";
		}

		try
		{
			sVersionString = utils.StrFormat("%s", AVS_env->Invoke("VersionString", AVSValue(&foo, 0)).AsString());
		}
		catch (IScriptEnvironment::NotFound)
		{
			sVersionString = "Cannot invoke \"VersionString\"";
		}

		string sInternalFunctions = "";
		try
		{
			foo = AVS_env->GetVar("$InternalFunctions$");
			sInternalFunctions = foo.AsString();
			utils.StrTokenize(sInternalFunctions, vInternalFunctions, " ", TRUE);
			sort(vInternalFunctions.begin(), vInternalFunctions.end());
		}
		catch(...)
		{
			sInternalFunctions = "";
		}

		string sDLLFunctions = "";
		if (!b_CustomPluginDir)
		{
			try
			{
				foo = AVS_env->GetVar("$PluginFunctions$");
				sDLLFunctions = foo.AsString();
				utils.StrTokenize(sDLLFunctions, vPluginFunctions, " ", TRUE);
				sort(vPluginFunctions.begin(), vPluginFunctions.end());
			}
			catch(...)
			{
				sDLLFunctions = "";
			}
		}

		foo = 0;
		AVS_env->DeleteScriptEnvironment();
		AVS_env = 0;
		AVS_linkage = 0;
	}
	catch (AvisynthError err)
	{
		bSuccess = FALSE;
		s_ErrorMsg = utils.StrFormat("%s", (PCSTR)err.msg);
	}
	catch (exception& ex)
	{
		bSuccess = FALSE;
		s_ErrorMsg = ex.what();
	}
	catch (...)
	{
		bSuccess = FALSE;
		s_ErrorMsg = utils.SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll:\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";
	}

	::FreeLibrary(hDLL);

	if (!bSuccess)
		return FALSE;

	EnumPluginDirs(b_CustomPluginDir);
	EnumPluginDLLs();
	TestLoadPlugins();

	return bSuccess;
}


void CAvisynthInfo::EnumPluginDirs(BOOL b_CustomPluginDir)
{
	vPluginDirs.empty();

	if (b_CustomPluginDir)
	{
		string sCustomPlugDir = BrowseDirectory();
		if (sCustomPlugDir != "INVALID")
		{
			vPluginDirs.push_back("Custom plugin directory :\t" + sCustomPlugDir);
			return;
		}
		else
		{
			vPluginDirs.push_back("Custom plugin directory :\tInvalid directory specified");
			return;
		}
	}

	vPluginDirs.empty();

	//- PluginDir+ in Software/Avisynth in HKEY_CURRENT_USER
	//- PluginDir+ in Software/Avisynth in HKEY_LOCAL_MACHINE
	//- PluginDir2_5 in Software/Avisynth in HKEY_CURRENT_USER
	//- PluginDir2_5 in Software/Avisynth in HKEY_LOCAL_MACHINE

	LONG lRes;
	HKEY hKeyResult;
	char szPlugDir[2050];
	DWORD dwType;
	DWORD dwBytes;

	hKeyResult = 0;
	lRes = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Avisynth", 0, KEY_READ, &hKeyResult);
	if (lRes == ERROR_SUCCESS)
	{
		memset(szPlugDir, 0, 2048);
		dwType = REG_SZ;
		dwBytes = 2048;
		if (RegQueryValueEx(hKeyResult, "PluginDir2_5", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes) == 0)
		{
			string sPluginDir(szPlugDir);
			utils.StrTrim(sPluginDir);

			if (sPluginDir != "")
			{
				if (PROCESS_64)
					vPluginDirs.push_back("PluginDir2_5 (HKCU, x64):\t" + sPluginDir);
				else
					vPluginDirs.push_back("PluginDir2_5 (HKCU, x86):\t" + sPluginDir);
			}
		}

		memset(szPlugDir, 0, 2048);
		dwType = REG_SZ;
		dwBytes = 2048;
		if (RegQueryValueEx(hKeyResult, "PluginDir+", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes) == 0)
		{
			string sPluginDir(szPlugDir);
			utils.StrTrim(sPluginDir);

			if ((sPluginDir != "") && (bIsAVSPlus))
			{
				if (PROCESS_64)
					vPluginDirs.push_back("PluginDir+   (HKCU, x64):\t" + sPluginDir);
				else
					vPluginDirs.push_back("PluginDir+   (HKCU, x86):\t" + sPluginDir);
			}
		}

		RegCloseKey(hKeyResult);
	}


	hKeyResult = 0;
	lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Avisynth", 0, KEY_READ, &hKeyResult);
	if (lRes == ERROR_SUCCESS)
	{
		memset(szPlugDir, 0, 2048);
		dwType = REG_SZ;
		dwBytes = 2048;
		if (RegQueryValueEx(hKeyResult, "PluginDir2_5", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes) == 0)
		{
			string sPluginDir(szPlugDir);
			utils.StrTrim(sPluginDir);

			if (sPluginDir != "")
			{
				if (PROCESS_64)
					vPluginDirs.push_back("PluginDir2_5 (HKLM, x64):\t" + sPluginDir);
				else
					vPluginDirs.push_back("PluginDir2_5 (HKLM, x86):\t" + sPluginDir);
			}
		}

		memset(szPlugDir, 0, 2048);
		dwType = REG_SZ;
		dwBytes = 2048;
		if (RegQueryValueEx(hKeyResult, "PluginDir+", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes) == 0)
		{
			string sPluginDir(szPlugDir);
			utils.StrTrim(sPluginDir);

			if ((sPluginDir != "") && (bIsAVSPlus))
			{
				if (PROCESS_64)
					vPluginDirs.push_back("PluginDir+   (HKLM, x64):\t" + sPluginDir);
				else
					vPluginDirs.push_back("PluginDir+   (HKLM, x86):\t" + sPluginDir);
			}
		}

		RegCloseKey(hKeyResult);
	}

	if (vPluginDirs.size() < 1)
		vPluginErrors.push_back("No plugin directory references found in the registry.\nPlugin auto-loading disabled.");

	return;
}


void CAvisynthInfo::EnumPluginDLLs()
{
	string sDir = "";
	string sMessage = "";
	BOOL bIs64BitPlugin = FALSE;
	set <string> mDuplicates;

	for (size_t uiPlugDir = 0; uiPlugDir < vPluginDirs.size(); uiPlugDir++)
	{
		sDir = vPluginDirs[uiPlugDir];
		if (sDir != "")
		{
			size_t spos = sDir.find(":\t");
			sDir = sDir.substr(spos + 2);

			WIN32_FIND_DATA fd;
			HANDLE hFind;
			string sFind =  sDir + "\\*.*";
			BOOL bRet = TRUE;
			string sCurrentFile = "";
			string sCurrentFileLC = "";
			hFind = FindFirstFile (sFind.c_str(), &fd);
			while (hFind != INVALID_HANDLE_VALUE && bRet)
			{
				if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				{
					sCurrentFile = sDir + "\\" + fd.cFileName;
					sCurrentFileLC = sCurrentFile;
					utils.StrToLC(sCurrentFileLC);

					if (mDuplicates.find(sCurrentFileLC) != mDuplicates.end())
					{
						bRet = FindNextFile(hFind, &fd);
						continue;
					}

					mDuplicates.insert(sCurrentFileLC);

					if (sCurrentFileLC.substr(sCurrentFileLC.length() - 4) == ".dll")
					{
						if (!GetPluginType(sCurrentFile, sMessage, bIs64BitPlugin))
						{
							if (sMessage != "")
								vPluginErrors.push_back(sMessage);
						}
						else
						{
							if (bIs64BitPlugin)
							{
								if (sMessage == "AVSC25")
									vPlugins.push_back("C 2.5 Plugins (64 Bit)|" + sCurrentFile);
								if (sMessage == "AVSC20")
									vPlugins.push_back("C 2.0 Plugins (64 Bit)|" + sCurrentFile);
								if (sMessage == "AVSCPP26")
									vPlugins.push_back("CPP 2.6 Plugins (64 Bit)|" + sCurrentFile);
								if (sMessage == "AVSCPP25")
									vPlugins.push_back("CPP 2.5 Plugins (64 Bit)|" + sCurrentFile);
								if (sMessage == "AVSCPP20")
									vPlugins.push_back("CPP 2.0 Plugins (64 Bit)|" + sCurrentFile);
								if (sMessage == "UNCATEGORIZED")
									vPlugins.push_back("Uncategorized DLLs (64 Bit)|" + sCurrentFile);
							}
							else
							{
								if (sMessage == "AVSC25")
									vPlugins.push_back("C 2.5 Plugins (32 Bit)|" + sCurrentFile);
								if (sMessage == "AVSC20")
									vPlugins.push_back("C 2.0 Plugins (32 Bit)|" + sCurrentFile);
								if (sMessage == "AVSCPP26")
									vPlugins.push_back("CPP 2.6 Plugins (32 Bit)|" + sCurrentFile);
								if (sMessage == "AVSCPP25")
									vPlugins.push_back("CPP 2.5 Plugins (32 Bit)|" + sCurrentFile);
								if (sMessage == "AVSCPP20")
									vPlugins.push_back("CPP 2.0 Plugins (32 Bit)|" + sCurrentFile);
								if (sMessage == "UNCATEGORIZED")
									vPlugins.push_back("Uncategorized DLLs (32 Bit)|" + sCurrentFile);
							}
						}
					}
					else if (sCurrentFileLC.substr(sCurrentFileLC.length() - 5) == ".avsi")
						vPlugins.push_back("Scripts (AVSI)|" + sCurrentFile);
					else
						vPlugins.push_back("Uncategorized files|" + sCurrentFile);
				}
				bRet = FindNextFile(hFind, &fd);
			}

			FindClose (hFind);
		}
	}

	sort(vPlugins.begin(), vPlugins.end(), CompareNoCase);

	return;
}


BOOL CAvisynthInfo::GetPluginType(string s_dll, string &s_Msg, BOOL &b_Is64BitDLL)
{
	BOOL bRet = TRUE;
	b_Is64BitDLL = FALSE;
	s_Msg = "UNCATEGORIZED";		

	LOADED_IMAGE li;
	BOOL bLoaded = MapAndLoad((LPSTR)s_dll.c_str(), NULL, &li,	TRUE,	TRUE);

	if (!bLoaded)
	{
		s_Msg = utils.SysErrorMessage();
		if (s_Msg != "")
			s_Msg = utils.StrFormat("Error loading \"%s\":\n%s", s_dll.c_str(), s_Msg.c_str());
		else
			s_Msg = utils.StrFormat("Error loading \"%s\"", s_dll.c_str());

		return FALSE;
	}

	try
	{
		_set_se_translator(SE_Translator);

		if (li.FileHeader->FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
		{
			b_Is64BitDLL = TRUE;
			if (!PROCESS_64)
			{
				UnMapAndLoad(&li);
				return TRUE;
			}
		}
		else
		{
			if (PROCESS_64)
			{
				UnMapAndLoad(&li);
				return TRUE;
			}
		}

		DWORD expVA = li.FileHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
		if (expVA == 0)
		{
			UnMapAndLoad(&li);
			return TRUE;
		}

		PIMAGE_EXPORT_DIRECTORY pExp = (PIMAGE_EXPORT_DIRECTORY)ImageRvaToVa(li.FileHeader, li.MappedAddress, expVA, NULL);
		if (pExp == 0)
		{
			UnMapAndLoad(&li);
			return TRUE;
		}

		DWORD rvaNames = pExp->AddressOfNames;
		DWORD *prvaNames = (DWORD*)ImageRvaToVa(li.FileHeader, li.MappedAddress, rvaNames, NULL);
		if (prvaNames == 0)
		{
			UnMapAndLoad(&li);
			return TRUE;
		}

		string sName = "";
		DWORD dwName = 0;
		for (dwName = 0; dwName < pExp->NumberOfNames; ++dwName)
		{
			sName = "";
			DWORD rvaName = prvaNames[dwName];
			sName = (char *)ImageRvaToVa(li.FileHeader, li.MappedAddress, rvaName, NULL);
			utils.StrToLC(sName);

			if (sName.find("avisynthplugininit3") != string::npos)
			{
				s_Msg = "AVSCPP26";
				break;
			}

			if (sName.find("avisynthplugininit2") != string::npos)
			{
				s_Msg = "AVSCPP25";
				break;
			}

			if (sName.find("avisynthplugininit") != string::npos)
				s_Msg = "AVSCPP20";

			if (sName.find("avisynth_c_plugin_init@4") != string::npos) //32 bit implied
			{
				s_Msg = "AVSC25";
				break;
			}

			if ((sName.find("avisynth_c_plugin_init") != string::npos) && (b_Is64BitDLL))
				s_Msg = "AVSC25";

			if ((sName.find("avisynth_c_plugin_init") != string::npos) && (!b_Is64BitDLL))
				s_Msg = "AVSC20";
		}
	}
	catch (exception& ex)
	{
		bRet = FALSE;
		s_Msg = utils.StrFormat("\"%s\"\n", s_dll.c_str()) + ex.what();
	}
	catch (...)
	{
		bRet = FALSE;
		s_Msg = utils.SysErrorMessage();
		if (s_Msg != "")
			s_Msg = utils.StrFormat("\"%s\"\n%s", s_dll.c_str(), s_Msg.c_str());
		else
			s_Msg = utils.StrFormat("Unknown exception:\n\"%s\"", s_dll.c_str());
	}

	UnMapAndLoad(&li);

	return bRet;
}


void CAvisynthInfo::TestLoadPlugins()
{
	HINSTANCE hDLL;
	if (sAVSDLL == "")
		hDLL = ::LoadLibrary("avisynth");
	else
		hDLL = ::LoadLibrary(sAVSDLL.c_str());

	if (!hDLL)
		return;

	string sPlugLoadError = "";
	string sPlugLoadErrorLC = "";
	string sNote = "";
	string sPlugin = "";
	string sPluginType = "";
	size_t uiPlugin = 0;
	size_t spos = 0;
	string sDependencies = "";
	string sFailedDependencies = "";
	string sHint = "";
	string sMsg = "";
	string sTemp = "";

	IScriptEnvironment *AVS_env = 0;

	for (uiPlugin = 0; uiPlugin < vPlugins.size(); uiPlugin++)
	{
		if (AVS_env == 0)
		{
			try
			{
				_set_se_translator(SE_Translator);
	
				CREATE_ENV *CreateEnvironment = (CREATE_ENV *)GetProcAddress(hDLL, "CreateScriptEnvironment");
				if (!CreateEnvironment)
				{
					::FreeLibrary(hDLL);
					return;
				}

				AVS_env = CreateEnvironment(iInterfaceVersion);

				if (!AVS_env)
				{
					::FreeLibrary(hDLL);
					return;
				}
			}
			catch (exception& ex)
			{
				::FreeLibrary(hDLL);
				string dummy = ex.what();
				return;
			}

			AVS_linkage = AVS_env->GetAVSLinkage();
		}	
	
		sPlugin = vPlugins[uiPlugin];
		spos = sPlugin.find("|");
		if (spos < 3)
			continue;

		sPluginType = sPlugin.substr(0, spos);

		if (sPluginType.find("Plugins") == string::npos)
			continue;

		sPlugin = sPlugin.substr(spos + 1);

		sDependencies = "";
		sFailedDependencies = "";
		sHint = "";
		GetDLLDependencies(sPlugin, sDependencies, sFailedDependencies, sHint);
		if (sDependencies != "")
			vPluginDependencies.push_back(sDependencies);

		sPlugLoadError = "";
		sNote = "";
		try
		{
			AVS_env->Invoke("LoadPlugin", sPlugin.c_str());
		}
		catch (AvisynthError err)
		{
			sPlugLoadError = utils.StrFormat("%s", (PCSTR)err.msg);
			utils.StrTrim(sPlugLoadError);
			sPlugLoadErrorLC = sPlugLoadError;
			utils.StrToLC(sPlugLoadErrorLC);

			if (sFailedDependencies != "")
			{
				sNote += utils.StrFormat("\n\nDependencies that could not be loaded:\n%s", sFailedDependencies.c_str());
				if (sHint != "")
					sNote += utils.StrFormat("\n\nNote: %s", sHint.c_str());
			}

			if ((sPlugLoadErrorLC.find("proc not found") != string::npos) || (sPlugLoadErrorLC.find("the specified procedure could not be found") != string::npos))
				sNote += "\n\nNote: You may need a newer OS version in order to use this plugin";

			if (sPluginType.substr(0, 5) == "C 2.5")
				sNote += "\n\nNote: C-Plugins must be loaded explicitly with \"LoadCPlugin()\"";

			if ((sPluginType.substr(0, 5) == "C 2.0") && (bIsAVSPlus))
				sNote += "\n\nNote: C 2.0 Plugins are not supported by Avisynth+";

			if ((sPluginType.substr(0, 7) == "CPP 2.0") && (bIsAVSPlus))
				sNote += "\n\nNote: CPP 2.0 Plugins are not supported by Avisynth+";

			sPlugLoadError += sNote;

			if (sPlugLoadError != "")
				vPluginErrors.push_back(sPlugLoadError);
		}

		if (((uiPlugin % 40) == 0) && (AVS_env != 0))
		{
			AVS_env->DeleteScriptEnvironment();
			AVS_env = 0;
			AVS_linkage = 0;
		}
	}

	if (AVS_env != 0)
	{
		AVS_env->DeleteScriptEnvironment();
		AVS_env = 0;
		AVS_linkage = 0;
	}

	FreeLibrary(hDLL);

	return;
}


void CAvisynthInfo::GetDLLDependencies(string s_dll, string &s_dependencies, string &s_failed_dependencies, string &s_hint)
{
	HANDLE hFile = NULL;
	HANDLE hFileMapping = NULL;
	LPBYTE lpbaseAddress = NULL;
	s_failed_dependencies = "";
	string sTemp = "";

	hFile = CreateFile(s_dll.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return;

	hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (hFileMapping == NULL || hFileMapping == INVALID_HANDLE_VALUE)
	{
		CloseHandle(hFile);
		return;
	}

	lpbaseAddress = (LPBYTE)MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
	if (lpbaseAddress == NULL)
	{
		CloseHandle(hFileMapping);
		CloseHandle(hFile);
		return;
	}

	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpbaseAddress;
	PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)(lpbaseAddress + pDosHeader->e_lfanew);
	DWORD rva_import_table = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
	if (rva_import_table == 0)
	{
		UnmapViewOfFile(lpbaseAddress);
		CloseHandle(hFileMapping);
		CloseHandle(hFile);
		return;
	}

	s_dependencies = s_dll + ":\n";
	string sDepDLL = "";
	PIMAGE_IMPORT_DESCRIPTOR pImageTable = (PIMAGE_IMPORT_DESCRIPTOR)ImageRvaToVa(pNtHeaders, lpbaseAddress, rva_import_table, NULL);
	IMAGE_IMPORT_DESCRIPTOR null_iid;
	IMAGE_THUNK_DATA null_thunk;
	memset(&null_iid, 0, sizeof(null_iid));
	memset(&null_thunk, 0, sizeof(null_thunk));

	for (int i = 0; memcmp(pImageTable + i, &null_iid, sizeof(null_iid)) != 0; i++)
	{
		LPCSTR szDepName = (LPCSTR)ImageRvaToVa(pNtHeaders, lpbaseAddress, pImageTable[i].Name,NULL);
		sTemp = utils.StrFormat("%s", szDepName);
		utils.StrToLC(sTemp);
		utils.StrTrim(sTemp);

		sDepDLL = utils.StrFormat("  %s\n", szDepName);
		s_dependencies += "  " + sDepDLL;

		if ((sTemp == "msvcm80.dll") || (sTemp == "msvcp80.dll") || (sTemp == "msvcr80.dll"))
		{
			if (!IsRuntimeInstalled("2005"))
				s_failed_dependencies += sDepDLL;

			continue;
		}

		if ((sTemp == "msvcm90.dll") || (sTemp == "msvcp90.dll") || (sTemp == "msvcr90.dll"))
		{
			if (!IsRuntimeInstalled("2008"))
				s_failed_dependencies += sDepDLL;

			continue;
		}

		HINSTANCE hDLL = ::LoadLibrary(szDepName);
		if (!hDLL)
			s_failed_dependencies += sDepDLL;
		else
			::FreeLibrary(hDLL);
	}

	if (lpbaseAddress)
		UnmapViewOfFile(lpbaseAddress);
	if (hFileMapping)
		CloseHandle(hFileMapping);
	if (hFile)
		CloseHandle(hFile);

	s_failed_dependencies.erase(s_failed_dependencies.find_last_not_of("\n") + 1);
	sTemp = s_failed_dependencies;
	utils.StrToLC(sTemp);

	s_hint = "";
	if ((sTemp.find("msvcm80.dll") != string::npos) ||
			(sTemp.find("msvcp80.dll") != string::npos) ||
			(sTemp.find("msvcr80.dll") != string::npos) ||
			(sTemp.find("vcomp.dll") != string::npos))
				s_hint += "Visual Studio 2005 Runtime doesn't seem to be installed\n";

	if ((sTemp.find("msvcm90.dll") != string::npos) ||
			(sTemp.find("msvcp90.dll") != string::npos) ||
			(sTemp.find("msvcr90.dll") != string::npos) ||
			(sTemp.find("vcomp90.dll") != string::npos))
				s_hint += "Visual Studio 2008 Runtime doesn't seem to be installed\n";

	if ((sTemp.find("msvcm100.dll") != string::npos) ||
			(sTemp.find("msvcp100.dll") != string::npos) ||
			(sTemp.find("msvcr100.dll") != string::npos) ||
			(sTemp.find("vcomp100.dll") != string::npos))
				s_hint += "Visual Studio 2010 Runtime doesn't seem to be installed\n";

	if ((sTemp.find("msvcm110.dll") != string::npos) ||
			(sTemp.find("msvcp110.dll") != string::npos) ||
			(sTemp.find("msvcr110.dll") != string::npos) ||
			(sTemp.find("vcomp110.dll") != string::npos))
				s_hint += "Visual Studio 2012 Runtime doesn't seem to be installed\n";

	if ((sTemp.find("msvcm120.dll") != string::npos) ||
			(sTemp.find("msvcp120.dll") != string::npos) ||
			(sTemp.find("msvcr120.dll") != string::npos) ||
			(sTemp.find("vcomp120.dll") != string::npos))
				s_hint += "Visual Studio 2013 Runtime doesn't seem to be installed\n";

	if ((sTemp.find("msvcm140.dll") != string::npos) ||
			(sTemp.find("msvcp140.dll") != string::npos) ||
			(sTemp.find("msvcr140.dll") != string::npos) ||
			(sTemp.find("vcomp140.dll") != string::npos) ||
			(sTemp.find("vcruntime140.dll") != string::npos) ||
			(sTemp.find("api-ms-win-crt") != string::npos))
				s_hint += "Visual Studio 2015/2017 Runtime doesn't seem to be installed\n";

	s_hint.erase(s_hint.find_last_not_of("\n") + 1);

	return;
}


BOOL CAvisynthInfo::IsRuntimeInstalled(string s_version)
{
	BOOL bInstalled = FALSE;
	LONG lRes;
	HKEY hKeyResult = 0;

	char szSubKey[2048] = "";
	vector<string> vSubKeys;
	DWORD dwBytes = 2050;
	DWORD dwIndex = 0;
	if (Is64BitOS())
		lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Classes\\Installer\\Products", 0, KEY_READ | KEY_WOW64_64KEY, &hKeyResult);
	else
		lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Classes\\Installer\\Products", 0, KEY_READ, &hKeyResult);

	if (lRes == ERROR_SUCCESS)
	{
		while (RegEnumKeyEx(hKeyResult, dwIndex, szSubKey, &dwBytes, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
		{
			vSubKeys.push_back(utils.StrFormat("%s", szSubKey));
			dwBytes = 2050;
			++dwIndex;
		}
	}
	RegCloseKey(hKeyResult);

	for (size_t uiKey = 0; uiKey < vSubKeys.size(); uiKey++)
	{
		string sKey = "Software\\Classes\\Installer\\Products\\" + vSubKeys[uiKey];
		hKeyResult = 0;

		if (Is64BitOS())
			lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, sKey.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &hKeyResult);
		else
			lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, sKey.c_str(), 0, KEY_READ, &hKeyResult);

		if (lRes == ERROR_SUCCESS)
		{
			char szValue[2048] = "";
			DWORD dwType = REG_SZ;
			dwBytes = 2050;
			if (RegQueryValueEx(hKeyResult, "ProductName", NULL, &dwType, (LPBYTE)szValue, &dwBytes) == 0)
			{
				string sValue(szValue);
				utils.StrToLC(sValue);

				BOOL bVersion = FALSE;
				BOOL bVC = FALSE;
				BOOL bRedist = FALSE;
				BOOL b64Bit = FALSE;

				if (sValue.find(s_version) != string::npos)
					bVersion = TRUE;
				if (sValue.find("visual c++") != string::npos)
					bVC = TRUE;
				if ((sValue.find("redistributable") != string::npos) || (sValue.find("runtime") != string::npos))
					bRedist = TRUE;
				if (sValue.find("x64") != string::npos)
					b64Bit = TRUE;
				if (bVersion && bVC && bRedist && !b64Bit && !bIs64BitAVSDLL)
					bInstalled = TRUE;
				if (bVersion && bVC && bRedist && b64Bit && bIs64BitAVSDLL)
					bInstalled = TRUE;
			}
		}
		RegCloseKey(hKeyResult);
	}

	return bInstalled;
}


__int64 CAvisynthInfo::FileSize(string s_file)
{
	WIN32_FIND_DATA fd;
	__int64 iSize = -1;

	HANDLE hFind = FindFirstFile(s_file.c_str(), &fd);
	if (hFind != INVALID_HANDLE_VALUE)
		iSize =	(((__int64)fd.nFileSizeHigh) << 32) + fd.nFileSizeLow;

	FindClose(hFind);

	return iSize;
}


BOOL CAvisynthInfo::Is64BitOS()
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


#endif //_AVISYNTHINFO_H

