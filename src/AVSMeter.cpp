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


#include "common.h"
#include "exception.h"
#include "AvisynthInfo.h"
#include "Utility.h"
#include "SysInfo.h"
#include "ProcessInfo.h"
#include "GPUInfo.h"
#include "Timer.h"

#define COLOR_DEFAULT           0
#define COLOR_AVSM_VERSION      FG_HRED | BG_BLACK
#define COLOR_AVISYNTH_VERSION  FG_HGREEN | BG_BLACK
#define COLOR_EMPHASIS          FG_WHITE | BG_BLACK
#define COLOR_ERROR             FG_HCYAN | BG_BLACK

#define REFRESH_INTERVAL              0.35  //seconds
#define MIN_TIME_PER_FRAMEINTERVAL   20.00  //milliseconds
#define MIN_RUNTIME                 500     //milliseconds

struct stSettings
{
	string    sLogDirectory;
	string    sAVSDLL;
	BOOL      bDisplayFPS;
	BOOL      bDisplayTPF;
	BOOL      bCreateLog;
	BOOL      bCreateCSV;
	BOOL      bPauseBeforeExit;
	__int64   iStartFrame;
	__int64   iStopFrame;
	string    sFrameRange;
	__int64   iTimeLimit;
	BOOL      bInvokeDistributor;
	BOOL      bAllowOnlyOneInstance;
	BOOL      bUseColor;
	BOOL      bGPUInfo;
	BYTE      nProcessPriority;
	BOOL      bConUseStdOut;
	string    sSystemDateTime;
	BOOL      bLogEstimatedTime;
	BOOL      bDisplayEfficiencyIndex;
	BOOL      bAutoCompleteExtension;
	BOOL      bLogFileDateTimeSuffix;
	BOOL      bSpecifyCustomPluginDir;
	BOOL      bLogUseFileSaveDialog;
} Settings;


struct stPerfData
{
	unsigned int  frame;
	float         fps_current;
	float         fps_average;
	double        cpu_usage;
	BYTE          gpu_usage;
	BYTE          vpu_usage;
	DWORD         process_memory;
	WORD          num_threads;
};

typedef IScriptEnvironment * __stdcall CREATE_ENV(int);

static CUtils utils;
static CTimer timer;
static CAvisynthInfo AvisynthInfo;
static CSysInfo sys;


unsigned int CalculateFrameInterval(string &s_avsfile, string &s_error);
string       CreateLogFile(string &s_avsfile, string &s_logbuffer, string &s_gpuinfo, vector<stPerfData> &cs_pdata, string &s_avserror, BOOL bNVVP, BOOL bOmitstPerfData);
string       CreateCSVFile(string &s_avsfile, vector<stPerfData> &cs_pdata, BOOL bNVVP);
string       ParseINIFile();
BOOL         WriteINIFile(string &s_inifile);
void         PrintUsage();
void         PollKeys();
void         PrintConsole(BOOL bUseStdOut, WORD wAttributes, const char *fmt, ...);
string       Pad(string s_line);



int main(int argc, char* argv[])
{
	UINT nPrevErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);

	string sINIRet = ParseINIFile();

	int iRet = 0;
	string sAVSMVersion = utils.GetFileVersion("");
	if (sAVSMVersion.length() > 5)
		sAVSMVersion = sAVSMVersion.substr(0, 5);

	if (PROCESS_64)
		sAVSMVersion += " (x64)";
	else
		sAVSMVersion += " (x86)";

	PrintConsole(Settings.bConUseStdOut, COLOR_AVSM_VERSION, "\nAVSMeter %s", sAVSMVersion.c_str());
	PrintConsole(Settings.bConUseStdOut, COLOR_AVSM_VERSION, " - Copyright (c) 2012-2018, Groucho2004\n");

	if (sINIRet != "")
	{
		PrintConsole(TRUE, COLOR_ERROR, sINIRet.c_str());
		PollKeys();
		return -1;
	}

	vector<stPerfData> perfdata;
	string sOutBuf = "";
	string sAVSFile = "";
	string sLogBuffer = "";
	BOOL   bRuntimeTooShort = FALSE;
	string sGPUInfo = "";
	BOOL bEarlyExit = TRUE;
	BOOL bInfoOnly = FALSE;
	BOOL bOmitPreScan = FALSE;
	BOOL bLogFunctions = FALSE;
	string sAVSError = "";
	unsigned int uiFrameInterval = 1;
	string sErrorMsg = "";
	BOOL bModeAVSInfo = FALSE;
	BOOL bCustomAVSDLLFromCL = FALSE;
	Settings.sSystemDateTime = "";

	BOOL CLSwitches_timelimit = FALSE;
	BOOL CLSwitches_priority = FALSE;
	BOOL CLSwitches_range = FALSE;
	BOOL CLSwitches_csv = FALSE;
	BOOL CLSwitches_info = FALSE;
	BOOL CLSwitches_o = FALSE;
	BOOL CLSwitches_c = FALSE;
	BOOL CLSwitches_lf = FALSE;

	if (Settings.bAllowOnlyOneInstance)
	{
		if (PROCESS_64)
			CreateMutex(NULL, TRUE, "__avsmeter64__single__instance__lock__");
		else
			CreateMutex(NULL, TRUE, "__avsmeter32__single__instance__lock__");

		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nOne instance of AVSMeter is already running\n");
			PollKeys();
			return -1;
		}
	}

	if (argc < 2)
	{
		PrintUsage();
		PollKeys();
		return -1;
	}

	string sArg = "";
	string sArgTest = "";
	string sTemp = "";
	size_t arg_len = 0;
	for (int iArg = 1; iArg < argc; iArg++)
	{
		sArg = argv[iArg];
		sArgTest = sArg;
		utils.StrTrim(sArgTest);
		utils.StrToLC(sArgTest);
		arg_len = sArgTest.length();

		if (sArgTest == "avsinfo")
		{
			bModeAVSInfo = TRUE;
			continue;
		}

		if (sArgTest == "-avsdll")
		{
			bCustomAVSDLLFromCL = TRUE;
			continue;
		}

		if (sArgTest == "-c")
		{
			CLSwitches_c = TRUE;
			Settings.bSpecifyCustomPluginDir = TRUE;
			continue;
		}

		if (sArgTest == "-o")
		{
			CLSwitches_o = TRUE;
			bOmitPreScan = TRUE;
			continue;
		}

		if (sArgTest == "-lf")
		{
			CLSwitches_lf = TRUE;
			bLogFunctions = TRUE;
			Settings.bCreateLog = TRUE;
			continue;
		}

		if ((sArgTest == "-i") || (sArgTest == "-info"))
		{
			CLSwitches_info = TRUE;
			bInfoOnly = TRUE;
			continue;
		}

		if ((sArgTest == "-l") || (sArgTest == "-log"))
		{
			Settings.bCreateLog = TRUE;
			continue;
		}

		if (sArgTest == "-csv")
		{
			CLSwitches_csv = TRUE;
			Settings.bCreateCSV = TRUE;
			continue;
		}

		if (sArgTest == "-gpu")
		{
			Settings.bGPUInfo = TRUE;
			continue;
		}

		if (sArgTest.substr(0, 7) == "-range=")
		{
			CLSwitches_range = TRUE;
			Settings.iStartFrame = 0xFFFFFFFFFF;
			Settings.iStopFrame = 0xFFFFFFFFFF;

			sTemp = sArgTest.substr(7);
			if (sTemp.length() < 3)
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter format: \"%s\"\n", sArg.c_str());
				PrintUsage();
				PollKeys();
				return -1;
			}

			size_t spos = sTemp.find(",");
			if ((spos < 1) || (spos > 27))
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter format: \"%s\"\n", sArg.c_str());
				PrintUsage();
				PollKeys();
				return -1;
			}

			Settings.sFrameRange = "FrameRange=" + sTemp;

			if ((utils.IsNumeric(sTemp.substr(0, spos))) && (utils.IsNumeric(sTemp.substr(spos + 1))))
			{
				Settings.iStartFrame = _atoi64(sTemp.substr(0, spos).c_str());
				Settings.iStopFrame = _atoi64(sTemp.substr(spos + 1).c_str());
			}
			else
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter format: \"%s\"\n", sArg.c_str());
				PrintUsage();
				PollKeys();
				return -1;
			}
			continue;
		}

		if (sArgTest.substr(0, 10) == "-priority=")
		{
			CLSwitches_priority = TRUE;
			sTemp = sArgTest.substr(10);
			if (utils.IsNumeric(sTemp))
			{
				Settings.nProcessPriority = (BYTE)atol(sTemp.c_str());
				if ((Settings.nProcessPriority < 1) || (Settings.nProcessPriority > 3))
				{
					PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be 1, 2 or 3\n", sArg.c_str());
					PollKeys();
					return -1;
				}
			}
			else
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be between 1, 2 or 3\n", sArg.c_str());
				PollKeys();
				return -1;
			}

			continue;
		}

		if (sArgTest.substr(0, 11) == "-timelimit=")
		{
			CLSwitches_timelimit = TRUE;
			sTemp = sArgTest.substr(11);
			if (utils.IsNumeric(sTemp))
			{
				Settings.iTimeLimit = atoi(sTemp.c_str());
				if ((Settings.iTimeLimit < 1) || (Settings.iTimeLimit > 999999))
				{
					PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be between \'1\' and \'999999\'\n", sArg.c_str());
					PollKeys();
					return -1;
				}
			}
			else
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be \'-1\' (no time limit) or between \'1\' and \'999999\'\n", sArg.c_str());
				PollKeys();
				return -1;
			}

			continue;
		}

		if (arg_len > 4)
		{
			if (sArgTest.substr(sArgTest.length() - 4) == ".avs")
			{
				LPTSTR lpPart;
				char szOut[MAX_PATH + 1];
				if (::GetFullPathName(argv[iArg], MAX_PATH, szOut, &lpPart))
				{
					sAVSFile = utils.StrFormat("%s", szOut);
					if (!utils.FileExists(sAVSFile))
					{
						PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nFile not found: \"%s\"\n", sArg.c_str());
						PollKeys();
						return -1;
					}
					else
						continue;
				}
			}
		}

		if ((Settings.bAutoCompleteExtension) && (sAVSFile == ""))
		{
			if (arg_len > 0)
			{
				sTemp = sArg + ".avs";
				LPTSTR lpPart;
				char szOut[MAX_PATH + 1];
				if (::GetFullPathName(sTemp.c_str(), MAX_PATH, szOut, &lpPart))
				{
					sAVSFile = utils.StrFormat("%s", szOut);
					if (!utils.FileExists(sAVSFile))
					{
						PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nFile not found: \"%s\"\n", sTemp.c_str());
						PollKeys();
						return -1;
					}
					else
						continue;
				}
			}
		}

		PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid argument: \"%s\"\n", sArg.c_str());
		PrintUsage();
		PollKeys();
		return -1;
	}


	if (bModeAVSInfo)
	{
		if (CLSwitches_info)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid switch in this context: \"-info [-i]\"\n");
			PrintUsage();
			PollKeys();
			return -1;
		}

		if (CLSwitches_timelimit)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid switch in this context: \"-timelimit\"\n");
			PrintUsage();
			PollKeys();
			return -1;
		}

		if (CLSwitches_priority)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid switch in this context: \"-priority\"\n");
			PrintUsage();
			PollKeys();
			return -1;
		}

		if (CLSwitches_range)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid switch in this context: \"-range\"\n");
			PrintUsage();
			PollKeys();
			return -1;
		}

		if (CLSwitches_csv)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid switch in this context: \"-csv\"\n");
			PrintUsage();
			PollKeys();
			return -1;
		}

		if (CLSwitches_o)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid switch in this context: \"-o\"\n");
			PrintUsage();
			PollKeys();
			return -1;
		}

		if (sAVSFile != "")
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nSpecifying a script together with \"avsinfo\" is pointless\n");
			PrintUsage();
			PollKeys();
			return -1;
		}
	}
	else
	{
		if (CLSwitches_c)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid switch in this context: \"-c\"\n");
			PrintUsage();
			PollKeys();
			return -1;
		}

		if (CLSwitches_lf)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid switch in this context: \"-lf\"\n");
			PrintUsage();
			PollKeys();
			return -1;
		}
	}

	if (Settings.nProcessPriority == 1)
		::SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
	else if (Settings.nProcessPriority == 3)
		::SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	else
		::SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);

	if (bCustomAVSDLLFromCL)
	{
		char szCustomAVSDLL[MAX_PATH + 1] = "";

		OPENFILENAME ofn;
		ofn.lStructSize       = sizeof(OPENFILENAME);
		ofn.hwndOwner         = ConsoleHWND;
		ofn.hInstance         = NULL;
		ofn.lpstrFilter       = "*.dll";
		ofn.lpstrCustomFilter = NULL;
		ofn.nMaxCustFilter    = 0;
		ofn.nFilterIndex      = 1;
		ofn.lpstrFile         = szCustomAVSDLL;
		ofn.nMaxFile          = MAX_PATH;
		ofn.lpstrFileTitle    = NULL;
		ofn.nMaxFileTitle     = NULL;
		ofn.lpstrInitialDir   = ".";
		ofn.lpstrTitle        = "Select Avisynth DLL";
		ofn.nFileOffset       = 0;
		ofn.nFileExtension    = 0;
		ofn.lpstrDefExt       = NULL;
		ofn.lCustData         = 0;
		ofn.lpfnHook          = NULL;
		ofn.lpTemplateName    = NULL;
		ofn.Flags             = OFN_HIDEREADONLY | OFN_EXPLORER;
			
		if (GetOpenFileName(&ofn) == 0)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nCannot open file or no avisynth.dll selected.\n");
			PollKeys();
			return -1;
		}

		Settings.sAVSDLL.assign(szCustomAVSDLL);
	}

	PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("Query Avisynth info...").c_str());

	BOOL bRet = FALSE;
	if (bModeAVSInfo)
		bRet = AvisynthInfo.GetInfo(Settings.sAVSDLL, Settings.bSpecifyCustomPluginDir, sErrorMsg);
	else
		bRet = AvisynthInfo.GetInfo(Settings.sAVSDLL, FALSE, sErrorMsg);

	if (!bRet)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
		PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n%s\n", sErrorMsg.c_str());
		PollKeys();
		return -1;
	}

	if (!bModeAVSInfo)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_AVISYNTH_VERSION, "\r%s", AvisynthInfo.sVersionString.c_str());
		PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, " (%s)\n", AvisynthInfo.sFileVersion.c_str());
	}

	CGPUInfo gpuinfo;
	if (Settings.bGPUInfo)
	{
		gpuinfo.GPUZInit();
		if (!gpuinfo.bInitialized)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nCannot initialize GPU-Z shared memory interface:\n%s\n", gpuinfo.sError.c_str());
			PollKeys();
			return -1;
		}
	}

	if (!timer.TestPerfCounter())
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
		PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nQueryPerformanceCounter() is not supported\n");
		PollKeys();
		return -1;
	}

	PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());

	if (bModeAVSInfo)
	{
		sOutBuf = utils.StrFormat("Log file created with:      AVSMeter %s\n", sAVSMVersion.c_str());
		sLogBuffer += sOutBuf;

		sLogBuffer += "\n[OS/Hardware info]\n";
		sOutBuf = utils.StrFormat("Operating system:           %s\n", sys.GetOSVersion().c_str());
		sLogBuffer += sOutBuf;

		if (sys.GetCPUInfo())
		{
			sOutBuf = utils.StrFormat("CPU brand string:           %s\n", sys.CPUBrandString.c_str());
			sLogBuffer += sOutBuf;

			sOutBuf = utils.StrFormat("CPU features:               %s\n", sys.CPUFeatures.c_str());
			sLogBuffer += sOutBuf;
			if (sys.CPUFeatures.find("*") != std::string::npos)
				sLogBuffer += "                            (* CPU feature not supported by the operating system)\n";
		}

		if (Settings.bGPUInfo)
		{
			sOutBuf = utils.StrFormat("\nVideo card:                 %s", gpuinfo.data.CardName.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("GPU version:                %s", gpuinfo.data.GPUName.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("Video memory size:          %s", gpuinfo.data.MemSize.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("OpenCL version:             %s", gpuinfo.data.OpenCLVersion.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("Graphics driver version:    %s\n", gpuinfo.data.DriverVersion.c_str());
			sLogBuffer += sOutBuf;
		}

		sLogBuffer += "\n\n[Avisynth info]";
		sOutBuf = utils.StrFormat("\nVersionString:              %s\n", AvisynthInfo.sVersionString.c_str());
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("VersionNumber:              %s\n", AvisynthInfo.sVersionNumber.c_str());
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("File / Product version:     %s / %s\n", AvisynthInfo.sFileVersion.c_str(), AvisynthInfo.sProductVersion.c_str());
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("Interface Version:          %d\n", AvisynthInfo.iInterfaceVersion);
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("Multi-threading support:    %s\n", AvisynthInfo.bIsMTVersion ? ("Yes") : ("No"));
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("Avisynth.dll location:      %s\n", utils.StrAnsiToOEM(AvisynthInfo.sDLLPath).c_str());
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("Avisynth.dll time stamp:    %s\n", AvisynthInfo.sTimeStamp.c_str());
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());


		if (AvisynthInfo.vPluginDirs.size() > 0)
		{
			string sPluginDir = "";
			for (unsigned int uiPlugDir = 0; uiPlugDir < AvisynthInfo.vPluginDirs.size(); uiPlugDir++)
			{
				sPluginDir = AvisynthInfo.vPluginDirs[uiPlugDir];
				if (sPluginDir != "")
				{
					size_t spos = sPluginDir.find(":\t");
					sPluginDir = sPluginDir.substr(0, spos + 1) + "   " + sPluginDir.substr(spos + 2);

					sOutBuf = utils.StrFormat("%s\n", sPluginDir.c_str());
					sLogBuffer += sOutBuf;
					PrintConsole(TRUE, COLOR_EMPHASIS, utils.StrAnsiToOEM(sOutBuf).c_str());
				}
			}
		}


		//Plugin info
		if (AvisynthInfo.vPlugins.size() > 0)
		{
			unsigned int uiPlugin = 0;
			string sPluginDLL = "";
			string sPluginType = "";
			string sOldPluginType = "";
			string sPluginVersion = "";
			size_t spos = 0;

			PrintConsole(TRUE, COLOR_DEFAULT, "\n");
			sLogBuffer += "\n";

			for (uiPlugin = 0; uiPlugin < AvisynthInfo.vPlugins.size(); uiPlugin++)
			{
				sPluginDLL = AvisynthInfo.vPlugins[uiPlugin];
				spos = sPluginDLL.find("|");
				sPluginType = sPluginDLL.substr(0, spos);
				utils.StrTrim(sPluginType);
				BYTE HColor = COLOR_AVSM_VERSION;

				if (sPluginType != sOldPluginType)
				{
					if ((sPluginType.find("32 Bit") != std::string::npos) && (PROCESS_64))
						HColor = BG_BLACK | FG_HMAGENTA;
					else if ((sPluginType.find("64 Bit") != std::string::npos) && (!PROCESS_64))
						HColor = BG_BLACK | FG_HMAGENTA;
					else
						HColor = COLOR_AVSM_VERSION;

					sOutBuf = utils.StrFormat("\n[%s]\n", sPluginType.c_str());
					sLogBuffer += sOutBuf;
					PrintConsole(TRUE, HColor, sOutBuf.c_str());
					sOldPluginType = sPluginType;
				}

				sPluginDLL = sPluginDLL.substr(spos + 1);

				sOutBuf = utils.StrFormat("%s", sPluginDLL.c_str());
				sLogBuffer += sOutBuf;
				PrintConsole(TRUE, COLOR_EMPHASIS, utils.StrAnsiToOEM(sOutBuf).c_str());

				sPluginVersion = utils.GetFileVersion(sPluginDLL);
				if (sPluginVersion != "n/a")
				{
					sOutBuf = utils.StrFormat("  [%s]\n", sPluginVersion.c_str());
					PrintConsole(TRUE, FG_HGREEN | BG_BLACK, utils.StrAnsiToOEM(sOutBuf).c_str());
				}
				else
				{
					sOutBuf = utils.StrFormat("  [%s]\n", utils.GetFileDateStamp(sPluginDLL).c_str());
					PrintConsole(TRUE, FG_HGREEN | BG_BLACK, utils.StrAnsiToOEM(sOutBuf).c_str());
				}

				sLogBuffer += sOutBuf;
			}
		}


		//Plugin errors
		if (AvisynthInfo.vPluginErrors.size() > 0)
		{
			string sPluginError = "";
			unsigned int err = 0;
			unsigned int errmaxlen = 0;
			unsigned int cpos = 0;

			for (err = 0; err < AvisynthInfo.vPluginErrors.size(); err++)
			{
				sPluginError = AvisynthInfo.vPluginErrors[err];
				for (cpos = 0; cpos < sPluginError.length(); cpos++)
				{
					if (sPluginError[cpos] == '\n')
						break;
				}
				if (cpos > errmaxlen)
					errmaxlen = cpos;
			}

			string sLineSep(errmaxlen, '_');

			sOutBuf = "\n\n\n[Plugin errors/warnings]\n";
			sLogBuffer += sOutBuf;
			PrintConsole(TRUE, COLOR_AVSM_VERSION, sOutBuf.c_str());
			sOutBuf = sLineSep + "\n";
			sLogBuffer += sOutBuf;
			PrintConsole(TRUE, COLOR_ERROR, sOutBuf.c_str());

			for (err = 0; err < AvisynthInfo.vPluginErrors.size(); err++)
			{
				sPluginError = AvisynthInfo.vPluginErrors[err];
				sOutBuf = utils.StrFormat("\n%s\n", sPluginError.c_str());
				sOutBuf += sLineSep + "\n";
				sLogBuffer += sOutBuf;
				PrintConsole(TRUE, COLOR_ERROR, utils.StrAnsiToOEM(sOutBuf).c_str());
			}
		}

		//Plugin Dependencies
		if (AvisynthInfo.vPluginDependencies.size() > 0)
		{
			if (AvisynthInfo.bIs64BitAVSDLL)
				sOutBuf = "\n\n\n[DLL dependencies (x64)]\n";
			else
				sOutBuf = "\n\n\n[DLL dependencies (x86)]\n";

			sLogBuffer += sOutBuf;

			for (unsigned int PDep = 0; PDep < AvisynthInfo.vPluginDependencies.size(); PDep++)
			{
				sOutBuf = utils.StrFormat("%s\n", AvisynthInfo.vPluginDependencies[PDep].c_str());
				sLogBuffer += sOutBuf;
			}
		}


		if (bLogFunctions)
		{
			//Internal Functions
			if (AvisynthInfo.vInternalFunctions.size() > 0)
			{
				string sFunction = "";
				sOutBuf = "\n\n\n[Internal (core) functions]\n";
				sLogBuffer += sOutBuf;
		
				for (unsigned int IFunc = 0; IFunc < AvisynthInfo.vInternalFunctions.size(); IFunc++)
				{
					sFunction = AvisynthInfo.vInternalFunctions[IFunc];
					sOutBuf = utils.StrFormat("    %s\n", sFunction.c_str());
					sLogBuffer += sOutBuf;
				}
			}

			//External (plugin) Functions
			if (AvisynthInfo.vPluginFunctions.size() > 0)
			{
				string sFunction = "";
				sOutBuf = "\n\n[External (plugin) functions]\n";
				sLogBuffer += sOutBuf;
	
				for (unsigned int PFunc = 0; PFunc < AvisynthInfo.vPluginFunctions.size(); PFunc++)
				{
					sFunction = AvisynthInfo.vPluginFunctions[PFunc];
					sOutBuf = utils.StrFormat("    %s\n", sFunction.c_str());
					sLogBuffer += sOutBuf;
				}
			}
		}


		if (Settings.bCreateLog)
		{
			if (Settings.bLogUseFileSaveDialog)
			{
				char szAVSInfoFile[MAX_PATH + 1] = "";

				if (PROCESS_64)
					sprintf(szAVSInfoFile, "avsinfo_x64.log");
				else
					sprintf(szAVSInfoFile, "avsinfo_x86.log");

				OPENFILENAME ofn;
				ofn.lStructSize       = sizeof(OPENFILENAME);
				ofn.hwndOwner         = ConsoleHWND;
				ofn.hInstance         = NULL;
				ofn.lpstrFilter       = "*.log";
				ofn.lpstrCustomFilter = NULL;
				ofn.nMaxCustFilter    = 0;
				ofn.nFilterIndex      = 1;
				ofn.lpstrFile         = szAVSInfoFile;
				ofn.nMaxFile          = MAX_PATH;
				ofn.lpstrFileTitle    = NULL;
				ofn.nMaxFileTitle     = NULL;
				ofn.lpstrInitialDir   = NULL;
				ofn.lpstrTitle        = "Save log file as...";
				ofn.nFileOffset       = 0;
				ofn.nFileExtension    = 0;
				ofn.lpstrDefExt       = NULL;
				ofn.lCustData         = 0;
				ofn.lpfnHook          = NULL;
				ofn.lpTemplateName    = NULL;
				ofn.Flags             = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_EXPLORER;
			
				if (GetSaveFileName(&ofn) == 0)
				{
					MessageBox(GetConsoleWindow(), "\rLog file not saved\r", "AVSMeter", MB_ICONEXCLAMATION);
				}
				else
				{
					ofstream hAVSInfoFile(szAVSInfoFile);
					if (hAVSInfoFile.is_open())
				  {
						hAVSInfoFile << sLogBuffer;
						hAVSInfoFile.flush();
						hAVSInfoFile.close();
					}
				}
			}
			else
			{
				if (Settings.sSystemDateTime == "")
					Settings.sSystemDateTime = sys.GetFormattedSystemDateTime();

	 			string sAVSInfoFile = "";
				char szPath[MAX_PATH + 1];
				if (::GetCurrentDirectory(MAX_PATH, szPath) > 0)
				{
					if (Settings.sLogDirectory == "")
					{
						if (Settings.bLogFileDateTimeSuffix)
						{
							if (PROCESS_64)
								sAVSInfoFile = utils.StrFormat("%s\\avsinfo_x64 [%s].log", szPath, Settings.sSystemDateTime.c_str());
							else
								sAVSInfoFile = utils.StrFormat("%s\\avsinfo_x86 [%s].log", szPath, Settings.sSystemDateTime.c_str());
						}
						else
						{
							if (PROCESS_64)
								sAVSInfoFile = utils.StrFormat("%s\\avsinfo_x64.log", szPath);
							else
								sAVSInfoFile = utils.StrFormat("%s\\avsinfo_x86.log", szPath);
						}
					}
					else
					{
						if (Settings.bLogFileDateTimeSuffix)
							sAVSInfoFile = utils.StrFormat("%s\\avsinfo [%s].log", Settings.sLogDirectory.c_str(), Settings.sSystemDateTime.c_str());
						else
							sAVSInfoFile = utils.StrFormat("%s\\avsinfo.log", Settings.sLogDirectory.c_str());
					}

					ofstream hAVSInfoFile(sAVSInfoFile.c_str());
					if (hAVSInfoFile.is_open())
				  {
						hAVSInfoFile << sLogBuffer;
						hAVSInfoFile.flush();
						hAVSInfoFile.close();
					}
				}
			}
		}

		PollKeys();
		return 0;
	}

	if (sAVSFile == "")
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nNo script file specified\n");
		PollKeys();
		return -1;
	}

	if (!bInfoOnly)
	{
		if (!bOmitPreScan)
		{
			uiFrameInterval = CalculateFrameInterval(sAVSFile, sErrorMsg);
			if (sErrorMsg != "")
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "%s\n", sErrorMsg.c_str());
				PollKeys();
				return -1;
			}
		}

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
	}

	HINSTANCE hDLL;
	if (Settings.sAVSDLL == "")
		hDLL = ::LoadLibrary("avisynth");
	else
		hDLL = ::LoadLibrary(Settings.sAVSDLL.c_str());

	if (!hDLL)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nCannot load avisynth.dll:\n%s\n", utils.SysErrorMessage().c_str());
		PollKeys();
		return -1;
	}


	IScriptEnvironment *AVS_env = 0;
	try
	{
		_set_se_translator(SE_Translator);

		CREATE_ENV *CreateEnvironment = (CREATE_ENV *)GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nFailed to load CreateScriptEnvironment()\n");
			PollKeys();
			return -1;
		}

		AVS_env = CreateEnvironment(AvisynthInfo.iInterfaceVersion);

		if (!AVS_env)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nCould not create IScriptenvironment\n");
			PollKeys();
			return -1;
		}

		AVS_linkage = AVS_env->GetAVSLinkage();
		AVSValue AVS_main;
		AVSValue AVS_temp;
		PClip AVS_clip;
		VideoInfo	AVS_vidinfo;

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("Loading script...").c_str());
		AVS_main = AVS_env->Invoke("Import", sAVSFile.c_str());
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());

		if (!AVS_main.IsClip())
			AVS_env->ThrowError("\"%s\":\nScript did not return a clip", sAVSFile.c_str());

		BOOL bIsSETMTVersion = TRUE;
		int iMTMode = 0;
		try
		{
			AVS_temp = AVS_env->Invoke("GetMTMode", false);
			iMTMode = AVS_temp.IsInt() ? AVS_temp.AsInt() : 0;
			if ((iMTMode > 0) && (iMTMode < 5) && Settings.bInvokeDistributor)
				AVS_main = AVS_env->Invoke("Distributor", AVS_main);
		}
		catch (IScriptEnvironment::NotFound)
		{
			bIsSETMTVersion = FALSE;
		}

		AVS_clip = AVS_main.AsClip();
		AVS_vidinfo = AVS_clip->GetVideoInfo();

		BOOL bAudioOnly = FALSE;
		if (!AVS_vidinfo.HasVideo() && AVS_vidinfo.HasAudio())
			bAudioOnly = TRUE;

		unsigned int uiFrames = 0;
		__int64 iMilliSeconds = 0;
		if (!bAudioOnly)
		{
			uiFrames = (unsigned int)AVS_vidinfo.num_frames;
			iMilliSeconds = (__int64)((((double)uiFrames * (double)AVS_vidinfo.fps_denominator * 1000.0) / (double)AVS_vidinfo.fps_numerator) + 0.5);
		}

		sOutBuf = utils.StrFormat("Log file created with:      AVSMeter %s", sAVSMVersion.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Script file:                %s", sAVSFile.c_str());
		sLogBuffer += sOutBuf + "\n";
		if (Settings.sLogDirectory != "")
		{
			sOutBuf = utils.StrFormat("Log file directory:         %s", Settings.sLogDirectory.c_str());
			sLogBuffer += sOutBuf + "\n";
		}

		sLogBuffer += "\n[OS/Hardware info]\n";
		sOutBuf = utils.StrFormat("Operating system:           %s\n", sys.GetOSVersion().c_str());
		sLogBuffer += sOutBuf;

		if (sys.GetCPUInfo())
		{
			sOutBuf = utils.StrFormat("CPU brand string:           %s\n", sys.CPUBrandString.c_str());
			sLogBuffer += sOutBuf;

			sOutBuf = utils.StrFormat("CPU features:               %s\n", sys.CPUFeatures.c_str());
			sLogBuffer += sOutBuf;
			if (sys.CPUFeatures.find("*") != std::string::npos)
				sLogBuffer += "                            (* CPU feature not supported by the operating system)\n";
		}

		if (Settings.bGPUInfo)
		{
			sOutBuf = utils.StrFormat("\nVideo card:                 %s", gpuinfo.data.CardName.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("GPU version:                %s", gpuinfo.data.GPUName.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("Video memory size:          %s", gpuinfo.data.MemSize.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("OpenCL version:             %s", gpuinfo.data.OpenCLVersion.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("Graphics driver version:    %s\n", gpuinfo.data.DriverVersion.c_str());
			sLogBuffer += sOutBuf;
		}

		sLogBuffer += "\n\n[Avisynth info]";
		sOutBuf = utils.StrFormat("\nVersionString:              %s", AvisynthInfo.sVersionString.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("VersionNumber:              %s", AvisynthInfo.sVersionNumber.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("File / Product version:     %s / %s", AvisynthInfo.sFileVersion.c_str(), AvisynthInfo.sProductVersion.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Interface Version:          %d", AvisynthInfo.iInterfaceVersion);
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Multi-threading support:    %s", AvisynthInfo.bIsMTVersion ? ("Yes") : ("No"));
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Avisynth.dll location:      %s", AvisynthInfo.sDLLPath.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Avisynth.dll time stamp:    %s", AvisynthInfo.sTimeStamp.c_str());
		sLogBuffer += sOutBuf + "\n";

		for (unsigned int uiPlugDir = 0; uiPlugDir < AvisynthInfo.vPluginDirs.size(); uiPlugDir++)
		{
			sTemp = AvisynthInfo.vPluginDirs[uiPlugDir];
			if (sTemp != "")
			{
				size_t spos = sTemp.find(":\t");
				sTemp = sTemp.substr(0, spos + 1) + "   " + sTemp.substr(spos + 2);

				sOutBuf = utils.StrFormat("%s", sTemp.c_str());
				sLogBuffer += sOutBuf + "\n";
			}
		}

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", Pad("").c_str());

		sLogBuffer += "\n\n[Clip info]\n";

		if (!bAudioOnly)
			sOutBuf = utils.StrFormat("Number of frames:          %11u", AVS_vidinfo.num_frames);
		else
			sOutBuf = "Number of frames:                  n/a";

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (!bAudioOnly)
			sOutBuf = utils.StrFormat("Length (hh:mm:ss.ms):  %s", timer.FormatTimeString(iMilliSeconds, TRUE).c_str());
		else
			sOutBuf = "Length (hh:mm:ss.ms):              n/a";

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (!bAudioOnly)
			sOutBuf = utils.StrFormat("Frame width:               %11u", AVS_vidinfo.width);
		else
			sOutBuf = "Frame width:                       n/a";

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (!bAudioOnly)
			sOutBuf = utils.StrFormat("Frame height:              %11u", AVS_vidinfo.height);
		else
			sOutBuf = "Frame height:                      n/a";

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (!bAudioOnly)
		{
			if (AVS_vidinfo.IsFieldBased())
			{
				if (AVS_vidinfo.IsTFF())
					sOutBuf = utils.StrFormat("Framerate:                 %11.3f (%u/%u, TFF)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
				else if (AVS_vidinfo.IsBFF())
					sOutBuf = utils.StrFormat("Framerate:                 %11.3f (%u/%u, BFF)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
				else
					sOutBuf = utils.StrFormat("Framerate:                 %11.3f (%u/%u)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
			}
			else
				sOutBuf = utils.StrFormat("Framerate:                 %11.3f (%u/%u)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
		}
		else
			sOutBuf = "Framerate:                         n/a";

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = "Colorspace:                 ";
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf;

		if (!AVS_vidinfo.HasVideo())
			sOutBuf = "       n/a";
		else
		{
			switch (AVS_vidinfo.pixel_type)
			{
				case VideoInfo::CS_YV411:       sOutBuf = "     YV411"; break;
				case VideoInfo::CS_YV24:        sOutBuf = "      YV24"; break;
				case VideoInfo::CS_YV16:        sOutBuf = "      YV16"; break;
				case VideoInfo::CS_Y8:          sOutBuf = "        Y8"; break;
				case VideoInfo::CS_YV12:        sOutBuf = "      YV12"; break;
				case VideoInfo::CS_I420:        sOutBuf = "      i420"; break;
				case VideoInfo::CS_YUY2:        sOutBuf = "      YUY2"; break;
				case VideoInfo::CS_BGR24:       sOutBuf = "     RGB24"; break;
				case VideoInfo::CS_BGR32:       sOutBuf = "     RGB32"; break;
				case VideoInfo::CS_YUV444P16:   sOutBuf = " YUV444P16"; break;
				case VideoInfo::CS_YUV422P16:   sOutBuf = " YUV422P16"; break;
				case VideoInfo::CS_YUV420P16:   sOutBuf = " YUV420P16"; break;
				case VideoInfo::CS_YUV444PS:    sOutBuf = "  YUV444PS"; break;
				case VideoInfo::CS_YUV422PS:    sOutBuf = "  YUV422PS"; break;
				case VideoInfo::CS_YUV420PS:    sOutBuf = "  YUV420PS"; break;
				case VideoInfo::CS_Y16:         sOutBuf = "       Y16"; break;
				case VideoInfo::CS_Y32:         sOutBuf = "       Y32"; break;
				case VideoInfo::CS_YUV444P10:   sOutBuf = " YUV444P10"; break;
				case VideoInfo::CS_YUV422P10:   sOutBuf = " YUV422P10"; break;
				case VideoInfo::CS_YUV420P10:   sOutBuf = " YUV420P10"; break;
				case VideoInfo::CS_Y10:         sOutBuf = "       Y10"; break;
				case VideoInfo::CS_YUV444P12:   sOutBuf = " YUV444P12"; break;
				case VideoInfo::CS_YUV422P12:   sOutBuf = " YUV422P12"; break;
				case VideoInfo::CS_YUV420P12:   sOutBuf = " YUV420P12"; break;
				case VideoInfo::CS_Y12:         sOutBuf = "       Y12"; break;
				case VideoInfo::CS_YUV444P14:   sOutBuf = " YUV444P14"; break;
				case VideoInfo::CS_YUV422P14:   sOutBuf = " YUV422P14"; break;
				case VideoInfo::CS_YUV420P14:   sOutBuf = " YUV420P14"; break;
				case VideoInfo::CS_Y14:         sOutBuf = "       Y14"; break;
				case VideoInfo::CS_BGR48:       sOutBuf = "     BGR48"; break;
				case VideoInfo::CS_BGR64:       sOutBuf = "     BGR64"; break;
				case VideoInfo::CS_RGBP:        sOutBuf = "      RGBP"; break;
				case VideoInfo::CS_RGBP10:      sOutBuf = "    RGBP10"; break;
				case VideoInfo::CS_RGBP12:      sOutBuf = "    RGBP12"; break;
				case VideoInfo::CS_RGBP14:      sOutBuf = "    RGBP14"; break;
				case VideoInfo::CS_RGBP16:      sOutBuf = "    RGBP16"; break;
				case VideoInfo::CS_RGBPS:       sOutBuf = "     RGBPS"; break;
				case VideoInfo::CS_RGBAP:       sOutBuf = "     RGBAP"; break;
				case VideoInfo::CS_RGBAP10:     sOutBuf = "   RGBAP10"; break;
				case VideoInfo::CS_RGBAP12:     sOutBuf = "   RGBAP12"; break;
				case VideoInfo::CS_RGBAP14:     sOutBuf = "   RGBAP14"; break;
				case VideoInfo::CS_RGBAP16:     sOutBuf = "   RGBAP16"; break;
				case VideoInfo::CS_RGBAPS:      sOutBuf = "    RGBAPS"; break;
				case VideoInfo::CS_YUVA444:     sOutBuf = "   YUVA444"; break;
				case VideoInfo::CS_YUVA422:     sOutBuf = "   YUVA422"; break;
				case VideoInfo::CS_YUVA420:     sOutBuf = "   YUVA420"; break;
				case VideoInfo::CS_YUVA444P10:  sOutBuf = "YUVA444P10"; break;
				case VideoInfo::CS_YUVA422P10:  sOutBuf = "YUVA422P10"; break;
				case VideoInfo::CS_YUVA420P10:  sOutBuf = "YUVA420P10"; break;
				case VideoInfo::CS_YUVA444P12:  sOutBuf = "YUVA444P12"; break;
				case VideoInfo::CS_YUVA422P12:  sOutBuf = "YUVA422P12"; break;
				case VideoInfo::CS_YUVA420P12:  sOutBuf = "YUVA420P12"; break;
				case VideoInfo::CS_YUVA444P14:  sOutBuf = "YUVA444P14"; break;
				case VideoInfo::CS_YUVA422P14:  sOutBuf = "YUVA422P14"; break;
				case VideoInfo::CS_YUVA420P14:  sOutBuf = "YUVA420P14"; break;
				case VideoInfo::CS_YUVA444P16:  sOutBuf = "YUVA444P16"; break;
				case VideoInfo::CS_YUVA422P16:  sOutBuf = "YUVA422P16"; break;
				case VideoInfo::CS_YUVA420P16:  sOutBuf = "YUVA420P16"; break;
				case VideoInfo::CS_YUVA444PS:   sOutBuf = " YUVA444PS"; break;
				case VideoInfo::CS_YUVA422PS:   sOutBuf = " YUVA422PS"; break;
				case VideoInfo::CS_YUVA420PS:   sOutBuf = " YUVA420PS"; break;
				case VideoInfo::CS_RAW32:       sOutBuf = "     RAW32"; break;
				case VideoInfo::CS_YUV9:        sOutBuf = "      YUV9"; break;
				default:                        sOutBuf = "   Unknown";
			}
		}

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s\n", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (bIsSETMTVersion)
		{
			sOutBuf = utils.StrFormat("Active MT Mode:                      %d", iMTMode);
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s\n", sOutBuf.c_str());
			sLogBuffer += sOutBuf + "\n";
		}

		sOutBuf = "Audio channels:    ";
		sLogBuffer += sOutBuf;
		if (AVS_vidinfo.HasAudio())
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s", sOutBuf.c_str());

		if (AVS_vidinfo.nchannels)
			sOutBuf = utils.StrFormat("%19d", AVS_vidinfo.nchannels);
		else
			sOutBuf = "                n/a";

		sLogBuffer += sOutBuf + "\n";
		if (AVS_vidinfo.HasAudio())
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s\n", sOutBuf.c_str());

		sOutBuf = "Audio bits/sample: ";
		sLogBuffer += sOutBuf;
		if (AVS_vidinfo.HasAudio())
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s", sOutBuf.c_str());

		switch (AVS_vidinfo.sample_type)
		{
			case SAMPLE_INT8:  sOutBuf = "                  8";         break;
			case SAMPLE_INT16: sOutBuf = "                 16";         break;
			case SAMPLE_INT24: sOutBuf = "                 24";         break;
			case SAMPLE_INT32: sOutBuf = "                 32";         break;
			case SAMPLE_FLOAT: sOutBuf = "                 32 (Float)"; break;
			default:           sOutBuf = "                n/a";
		}

		sLogBuffer += sOutBuf + "\n";
		if (AVS_vidinfo.HasAudio())
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s\n", sOutBuf.c_str());

		sOutBuf = "Audio sample rate: ";
		sLogBuffer += sOutBuf;
		if (AVS_vidinfo.HasAudio())
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s", sOutBuf.c_str());

		if (AVS_vidinfo.audio_samples_per_second)
			sOutBuf = utils.StrFormat("%19d", AVS_vidinfo.audio_samples_per_second);
		else
			sOutBuf = "                n/a";

		sLogBuffer += sOutBuf + "\n";
		if (AVS_vidinfo.HasAudio())
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s\n", sOutBuf.c_str());

		sOutBuf = "Audio samples:     ";
		sLogBuffer += sOutBuf;
		if (AVS_vidinfo.HasAudio())
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s", sOutBuf.c_str());

		if (AVS_vidinfo.num_audio_samples)
			sOutBuf = utils.StrFormat("%19I64d", AVS_vidinfo.num_audio_samples);
		else
			sOutBuf = "                n/a";

		sLogBuffer += sOutBuf + "\n";
		if (AVS_vidinfo.HasAudio())
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s\n", sOutBuf.c_str());

		if (bInfoOnly)
		{
			AVS_clip = 0;
			AVS_main = 0;
			AVS_temp = 0;
			AVS_env->DeleteScriptEnvironment();
			AVS_env = 0;

			if (Settings.bCreateLog)
			{
				string sLogRet = CreateLogFile(sAVSFile, sLogBuffer, sGPUInfo, perfdata, sAVSError, FALSE, bRuntimeTooShort);
				if (sLogRet != "")
					PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, sLogRet.c_str());
			}

			AVS_linkage = 0;
			::FreeLibrary(hDLL);

			PollKeys();
			return 0;
		}

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");

		if (!AVS_vidinfo.HasVideo())
			AVS_env->ThrowError("Script did not return a video clip:\n%s", sAVSFile.c_str());

		unsigned int uiFramesToProcess = 0;
		unsigned int uiFirstFrame = 0;
		unsigned int uiLastFrame = uiFrames - 1;

		if (Settings.iStopFrame == -1)
			Settings.iStopFrame = (__int64)uiFrames - 1;

		if ((Settings.iStartFrame >= 0) && (Settings.iStartFrame <= Settings.iStopFrame) && (Settings.iStopFrame < (__int64)uiFrames))
		{
			uiFirstFrame = (unsigned int)Settings.iStartFrame;
			uiLastFrame = (unsigned int)Settings.iStopFrame;
			uiFramesToProcess = uiLastFrame - uiFirstFrame + 1;

			sOutBuf = utils.StrFormat("Frame (current | last):         %u | %u", uiFirstFrame, uiLastFrame);
			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\r", Pad(sOutBuf).c_str());
		}
		else
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", Pad("").c_str());
			AVS_env->ThrowError("Invalid frame range specified:\n\"%s\"\n", Settings.sFrameRange.c_str());
		}

		while ((uiFramesToProcess / uiFrameInterval) > 100000)
			uiFrameInterval *= 10;

		while ((uiFrameInterval * 10) > uiFramesToProcess)
			uiFrameInterval /= 10;

		perfdata.reserve((2 * uiFramesToProcess) / uiFrameInterval);

		unsigned int uiFramesRead = 0;
		BOOL bFirstScr = TRUE;
		__int64 iElapsedMS = 0;
		__int64 iEstimatedMS = 0;

		CProcessInfo processinfo;

		DWORD dwMemPeakMB = 0;
		DWORD dwMemCurrentMB = 0;

		double dCPUUsageCur = 0;
		double dCPUUsageAcc = 0;
		double dCPUUsageAvg = 0;
		unsigned int uiGPUUsageCur = 0;
		unsigned int uiGPUUsageAcc = 0;
		unsigned int uiGPUUsageAvg = 0;
		unsigned int uiVPUUsageCur = 0;
		unsigned int uiVPUUsageAcc = 0;
		unsigned int uiVPUUsageAvg = 0;
		unsigned int uiIntervalCounter = 0;

		unsigned int uiCurrentFrame = 0;
		double dFPSAverage = 0.0;
		double dFPSCurrent = 0.0;
		double dFPSMin = 1.0e+20;
		double dFPSMax = 0.0;

		if (Settings.bGPUInfo)
		{
			gpuinfo.ReadSensors();
			if (gpuinfo.sensors.ReadError)
				AVS_env->ThrowError("Error reading GPU sensors\n");
		}

		processinfo.Update();

		double dStartTime = timer.GetTimer();
		double dCurrentTime = dStartTime;
		double dLastDisplayTime = dStartTime;
		double dLastIntervalTime = dStartTime;

		unsigned int uiCursorOffset = 0;
		for (uiCurrentFrame = uiFirstFrame; uiCurrentFrame <= uiLastFrame; uiCurrentFrame++)
		{
			PVideoFrame src_frame = AVS_clip->GetFrame(uiCurrentFrame, AVS_env);
			++uiFramesRead;

			if (((uiFramesRead % uiFrameInterval) != 0) && (uiFramesRead != uiFramesToProcess))
				continue;

			dCurrentTime = timer.GetTimer();

			processinfo.Update();

			++uiIntervalCounter;

			if (Settings.bGPUInfo)
			{
				gpuinfo.ReadSensors();
				if (gpuinfo.sensors.ReadError)
					AVS_env->ThrowError("Error reading GPU sensors\n");

				uiGPUUsageCur = (unsigned int)gpuinfo.sensors.GPULoad;
				uiGPUUsageAcc += uiGPUUsageCur;
				uiGPUUsageAvg = (unsigned int)(((double)uiGPUUsageAcc / (double)uiIntervalCounter) + 0.5);
				uiVPUUsageCur = (unsigned int)gpuinfo.sensors.VPULoad;
				uiVPUUsageAcc += uiVPUUsageCur;
				uiVPUUsageAvg = (unsigned int)(((double)uiVPUUsageAcc / (double)uiIntervalCounter) + 0.5);
			}

			iElapsedMS = (__int64)(((dCurrentTime - dStartTime) * 1000.0) + 0.5);
			iEstimatedMS = (__int64)((double)uiFramesToProcess * (double)iElapsedMS / (double)uiFramesRead);

			dCPUUsageCur = processinfo.dCPUUsage;
			dCPUUsageAcc += dCPUUsageCur;
			dCPUUsageAvg = dCPUUsageAcc / (double)uiIntervalCounter;

			dwMemCurrentMB = processinfo.dwMemMB;

			if (dwMemCurrentMB > dwMemPeakMB)
				dwMemPeakMB = dwMemCurrentMB;

			dFPSAverage = (double)uiFramesRead / (dCurrentTime - dStartTime);

			if ((uiFramesRead % uiFrameInterval) != 0)
				continue;

			if ((dCurrentTime - dLastIntervalTime) > 0.000001)
				dFPSCurrent = (double)uiFrameInterval / (dCurrentTime - dLastIntervalTime);
			else
				dFPSCurrent = (double)uiFrameInterval / 0.000001;

			if (dFPSCurrent > dFPSMax)
				dFPSMax = dFPSCurrent;
			if (dFPSCurrent < dFPSMin)
				dFPSMin = dFPSCurrent;

			stPerfData pdata;
			pdata.frame = uiCurrentFrame;
			pdata.fps_current = (float)dFPSCurrent;
			pdata.fps_average = (float)dFPSAverage;
			pdata.cpu_usage = processinfo.dCPUUsage;

			if (Settings.bGPUInfo)
			{
				pdata.gpu_usage = gpuinfo.sensors.GPULoad;
				pdata.vpu_usage = gpuinfo.sensors.VPULoad;
			}
			else
			{
				pdata.gpu_usage = 0;
				pdata.vpu_usage = 0;
			}

			pdata.num_threads = processinfo.wThreadCount;
			pdata.process_memory = dwMemCurrentMB;
			perfdata.push_back(pdata);

			dLastIntervalTime = dCurrentTime;

			if ((dCurrentTime - dLastDisplayTime) < REFRESH_INTERVAL)
				continue;

			dLastDisplayTime = dCurrentTime;

			if (!bFirstScr)
			{
				utils.CursorUp(uiCursorOffset);
				uiCursorOffset = 0;
			}

			bFirstScr = FALSE;
			bEarlyExit = FALSE;

			sOutBuf = utils.StrFormat("Frame (current | last):         %u | %u", uiCurrentFrame + 1, uiLastFrame);
			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
			++uiCursorOffset;

			if (Settings.bDisplayFPS)
			{
				sOutBuf = utils.StrFormat("FPS (cur | min | max | avg):    %s | %s | %s | %s", utils.StrFormatFPS(dFPSCurrent).c_str(), utils.StrFormatFPS(dFPSMin).c_str(), utils.StrFormatFPS(dFPSMax).c_str(), utils.StrFormatFPS(dFPSAverage).c_str());
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
				++uiCursorOffset;
			}

			if (Settings.bDisplayTPF)
			{
				sOutBuf = utils.StrFormat("TPF (cur | max | min | avg):    %s | %s | %s | %s ms", utils.StrFormatTPF(1000.0 / dFPSCurrent).c_str(), utils.StrFormatTPF(1000.0 / dFPSMin).c_str(), utils.StrFormatTPF(1000.0 / dFPSMax).c_str(), utils.StrFormatTPF(1000.0 / dFPSAverage).c_str());
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
				++uiCursorOffset;
			}

			sOutBuf = utils.StrFormat("Process memory usage:           %u MiB", dwMemCurrentMB);
			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
			++uiCursorOffset;

			sOutBuf = utils.StrFormat("Thread count:                   %u", processinfo.wThreadCount);
			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
			++uiCursorOffset;

			sOutBuf = utils.StrFormat("CPU usage (current | average):  %.1f%% | %.1f%%", dCPUUsageCur, dCPUUsageAvg);
			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
			++uiCursorOffset;

			if (Settings.bDisplayEfficiencyIndex)
			{
				sOutBuf = utils.StrFormat("Efficiency index:               %s", utils.StrFormatTPF(dFPSAverage / dCPUUsageAvg).c_str());
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
				++uiCursorOffset;
			}

			if (Settings.bGPUInfo)
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");
				++uiCursorOffset;

				sOutBuf = utils.StrFormat("GPU usage (current | average):  %u%% | %u%%", uiGPUUsageCur, uiGPUUsageAvg);
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
				++uiCursorOffset;

				if (gpuinfo.data.NVVPU)
				{
					sOutBuf = utils.StrFormat("VPU usage (current | average):  %u%% | %u%%", uiVPUUsageCur, uiVPUUsageAvg);
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
					++uiCursorOffset;
				}

				if (gpuinfo.data.GeneralMem)
				{
					sOutBuf = utils.StrFormat("GPU memory usage:               %u MiB", gpuinfo.sensors.MemoryUsedGeneral);
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
					++uiCursorOffset;
				}

				if (gpuinfo.data.DedicatedMem)
				{
					sOutBuf = utils.StrFormat("GPU memory usage (Dedicated):   %u MiB", gpuinfo.sensors.MemoryUsedDedicated);
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
					++uiCursorOffset;
				}

				if (gpuinfo.data.DynamicMem)
				{
					sOutBuf = utils.StrFormat("GPU memory usage (Dynamic):     %u MiB", gpuinfo.sensors.MemoryUsedDynamic);
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
					++uiCursorOffset;
				}
			}

			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");
			++uiCursorOffset;

			sOutBuf = utils.StrFormat("Time (elapsed | estimated):     %s | %s", timer.FormatTimeString(iElapsedMS, FALSE).c_str(), timer.FormatTimeString(iEstimatedMS, FALSE).c_str());
			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
			++uiCursorOffset;

			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\nPress \'Esc\' to cancel the process...\n\n");
			++uiCursorOffset;
			++uiCursorOffset;
			++uiCursorOffset;

			if (Settings.iTimeLimit != -1)
			{
				if (iElapsedMS >= (Settings.iTimeLimit * 1000))
				{
					uiLastFrame = uiCurrentFrame;
					break;
				}
			}

			if (_kbhit())
			{
				if (_getch() == 0x1B) //ESC
				{
					uiLastFrame = uiCurrentFrame;
					break;
				}
			}
		}

		processinfo.CloseProcess();

		if (Settings.bGPUInfo)
			gpuinfo.GPUZRelease();

		sLogBuffer += "\n\n[Runtime info]\n";

		if (iElapsedMS >= MIN_RUNTIME)
		{
			if (!bFirstScr)
				utils.CursorUp(uiCursorOffset);

			if (uiFirstFrame == uiLastFrame)
			{
				if (uiFirstFrame == 0)
					sOutBuf = utils.StrFormat("Frames processed:               %u", uiFramesRead);
				else
					sOutBuf = utils.StrFormat("Frames processed:               %u (%u)", uiFramesRead, uiFirstFrame);
			}
			else
				sOutBuf = utils.StrFormat("Frames processed:               %u (%u - %u)", uiFramesRead, uiFirstFrame, uiLastFrame);

			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			if (uiFramesRead >= uiFrameInterval)
			{
				if (Settings.bDisplayFPS)
				{
					sOutBuf = utils.StrFormat("FPS (min | max | average):      %s | %s | %s", utils.StrFormatFPS(dFPSMin).c_str(), utils.StrFormatFPS(dFPSMax).c_str(), utils.StrFormatFPS(dFPSAverage).c_str());
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
					sLogBuffer += sOutBuf + "\n";
				}

				if (Settings.bDisplayTPF)
				{
					sOutBuf = utils.StrFormat("TPF (max | min | average):      %s | %s | %s ms", utils.StrFormatTPF(1000.0 / dFPSMin).c_str(), utils.StrFormatTPF(1000.0 / dFPSMax).c_str(), utils.StrFormatTPF(1000.0 / dFPSAverage).c_str());
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
					sLogBuffer += sOutBuf + "\n";
				}

				sOutBuf = utils.StrFormat("Process memory usage (max):     %u MiB", dwMemPeakMB);
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
				sLogBuffer += sOutBuf + "\n";

				sOutBuf = utils.StrFormat("Thread count:                   %u", processinfo.wThreadCount);
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
				sLogBuffer += sOutBuf + "\n";

				sOutBuf = utils.StrFormat("CPU usage (average):            %.1f%%", dCPUUsageAvg);
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
				sLogBuffer += sOutBuf + "\n";

				if (Settings.bDisplayEfficiencyIndex)
				{
					sOutBuf = utils.StrFormat("Efficiency index:               %s", utils.StrFormatTPF(dFPSAverage / dCPUUsageAvg).c_str());
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
					sLogBuffer += sOutBuf + "\n";
				}

				if (Settings.bGPUInfo)
				{
					PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");
					sLogBuffer += "\n";

					sOutBuf = utils.StrFormat("GPU usage (average):            %u%%", uiGPUUsageAvg);
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
					sLogBuffer += sOutBuf + "\n";

					if (gpuinfo.data.NVVPU)
					{
						sOutBuf = utils.StrFormat("VPU usage (average):            %u%%", uiVPUUsageAvg);
						PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
						sLogBuffer += sOutBuf + "\n";
					}

					if (gpuinfo.data.GeneralMem)
					{
						sOutBuf = utils.StrFormat("GPU memory usage:               %u MiB", gpuinfo.sensors.MemoryUsedGeneral);
						PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
						sLogBuffer += sOutBuf + "\n";
					}

					if (gpuinfo.data.DedicatedMem)
					{
						sOutBuf = utils.StrFormat("GPU memory usage (Dedicated):   %u MiB", gpuinfo.sensors.MemoryUsedDedicated);
						PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
						sLogBuffer += sOutBuf + "\n";
					}

					if (gpuinfo.data.DynamicMem)
					{
						sOutBuf = utils.StrFormat("GPU memory usage (Dynamic):     %u MiB", gpuinfo.sensors.MemoryUsedDynamic);
						PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
						sLogBuffer += sOutBuf + "\n";
					}
				}

				PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");
				sLogBuffer += "\n";
			}

			if ((Settings.bLogEstimatedTime) && (uiFramesRead < uiFramesToProcess))
				sOutBuf = utils.StrFormat("Time (elapsed | estimated):     %s | %s", timer.FormatTimeString(iElapsedMS, FALSE).c_str(), timer.FormatTimeString(iEstimatedMS, FALSE).c_str());
			else
				sOutBuf = utils.StrFormat("Time (elapsed):                 %s", timer.FormatTimeString(iElapsedMS, FALSE).c_str());

			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
			utils.CursorUp(2);
		}
		else
		{
			if (!bFirstScr)
			{
				for (unsigned int u = 0; u < uiCursorOffset; u++)
				{
					PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", Pad("").c_str());
					utils.CursorUp(1);
				}
			}

			sOutBuf = "Script runtime is too short for meaningful measurements";
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\r%s\n", Pad(sOutBuf).c_str());
			sLogBuffer += sOutBuf;
			bRuntimeTooShort = TRUE;
		}

		AVS_clip = 0;
		AVS_main = 0;
		AVS_temp = 0;
		AVS_env->DeleteScriptEnvironment();
		AVS_env = 0;
		AVS_linkage = 0;
	}
	catch (AvisynthError err)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
		sAVSError = utils.StrFormat("%s", (PCSTR)err.msg);
		iRet = -1;

		if (bEarlyExit == FALSE)
		{
			utils.CursorUp(2);
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
			utils.CursorUp(2);
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\r%s\n", sAVSError.c_str());
		}
		else
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n%s\n", sAVSError.c_str());
	}
	catch (exception& ex)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
		iRet = -1;
		if (bEarlyExit == FALSE)
		{
			utils.CursorUp(2);
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
			utils.CursorUp(2);
		}

		sAVSError = ex.what();
		if (sAVSError == "")
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nUnknown exception\n");
		else
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n%s\n", sAVSError.c_str());

	}
	catch (...)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
		iRet = -1;
		if (bEarlyExit == FALSE)
		{
			utils.CursorUp(2);
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
			utils.CursorUp(2);
		}

		sAVSError = utils.SysErrorMessage();
		if (sAVSError == "")
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nUnknown exception\n");
		else
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n%s\n", sAVSError.c_str());
	}

	::FreeLibrary(hDLL);

	if (Settings.bCreateLog)
	{
		string sr = CreateLogFile(sAVSFile, sLogBuffer, sGPUInfo, perfdata, sAVSError, gpuinfo.data.NVVPU, bRuntimeTooShort);
		if (sr != "")
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, sr.c_str());
			PollKeys();
			return -1;
		}
	}

	if (Settings.bCreateCSV && !bRuntimeTooShort && (sAVSError == ""))
	{
		string cr = CreateCSVFile(sAVSFile, perfdata, gpuinfo.data.NVVPU);
		if (cr != "")
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, cr.c_str());
			PollKeys();
			return -1;
		}
	}

	SetErrorMode(nPrevErrorMode);

	PollKeys();

	return iRet;
}


string ParseINIFile()
{
	string sRet = "";
	string sProgramPath = "";
	string sINIFile = "";
	string sTemp = "";

	char szPath[MAX_PATH + 1];
	if (::GetModuleFileName(NULL, szPath, MAX_PATH) > 0)
	{
		sProgramPath = utils.StrFormat("%s", szPath);
		size_t i = 0;
		for (i = (sProgramPath.length() - 1); i > 0; i--)
		{
			if (sProgramPath[i] == '\\')
				break;
		}
		sProgramPath = sProgramPath.substr(0, i);
		sINIFile = sProgramPath + "\\AVSMeter.ini";
	}

	string sCurrentLine = "";

	//defaults
	Settings.sLogDirectory = "";
	Settings.sAVSDLL = "";
	Settings.bDisplayFPS = TRUE;
	Settings.bDisplayTPF = FALSE;
	Settings.bCreateLog = FALSE;
	Settings.bCreateCSV = FALSE;
	Settings.bLogFileDateTimeSuffix = FALSE;
	Settings.bPauseBeforeExit = FALSE;
	Settings.iStartFrame = 0;
	Settings.iStopFrame = -1;
	Settings.bInvokeDistributor = TRUE;
	Settings.bAllowOnlyOneInstance = TRUE;
	Settings.bUseColor = TRUE;
	Settings.bGPUInfo = FALSE;
	Settings.nProcessPriority = 2;
	Settings.bConUseStdOut = FALSE;
	Settings.iTimeLimit = -1;
	Settings.bDisplayEfficiencyIndex = FALSE;
	Settings.bLogEstimatedTime = FALSE;
	Settings.bAutoCompleteExtension = FALSE;
	Settings.bLogUseFileSaveDialog = FALSE;

	if (!utils.FileExists(sINIFile)) //No ini file present, create the file with defaults
	{
		if (!WriteINIFile(sINIFile))
	  {
			sRet = utils.StrFormat("\nCannot create \"%s\"\n", sINIFile.c_str());
			return sRet;
		}

		return "";
	}

	ifstream hINIFile(sINIFile.c_str());
	if (!hINIFile.is_open())
	{
		sRet = utils.StrFormat("\nCannot open \"%s\"\n", sINIFile.c_str());
		return sRet;
	}

	string sOrgLine = "";
	sCurrentLine = "";
	size_t spos = 0;
	int iBoolValue = 0;

	while (getline(hINIFile, sCurrentLine))
	{
		utils.StrTrim(sCurrentLine);
		sOrgLine = sCurrentLine;
		utils.StrToLC(sCurrentLine);
		utils.StrTrim(sCurrentLine);

		if ((sCurrentLine.substr(0, 1) == "#") || (sCurrentLine == ""))
			continue;

		if (sCurrentLine.substr(0, 12) == "logdirectory")
		{
			Settings.sLogDirectory = sOrgLine.substr(13);
			Settings.sLogDirectory.erase(Settings.sLogDirectory.find_last_not_of("\\") + 1); //remove trailing bs if present
			utils.StrTrim(Settings.sLogDirectory);
			if ((!utils.DirectoryExists(Settings.sLogDirectory)) && (Settings.sLogDirectory != ""))
			{
				sRet = utils.StrFormat("\nLog directory specified in AVSMeter.ini does not exist:\n\"%s\"\n", Settings.sLogDirectory.c_str());
				return sRet;
			}
			continue;
		}
		if (sCurrentLine.substr(0, 6) == "avsdll")
		{
			Settings.sAVSDLL = sOrgLine.substr(7);
			utils.StrTrim(Settings.sAVSDLL);
			if ((!utils.FileExists(Settings.sAVSDLL)) && (Settings.sAVSDLL != ""))
			{
				sRet = utils.StrFormat("\nThe file specified in AVSMeter.ini does not exist:\nAVSDLL=%s\n", Settings.sAVSDLL.c_str());
				return sRet;
			}
			continue;
		}

		if (sCurrentLine.substr(0, 15) == "processpriority")
		{
			sTemp = sCurrentLine.substr(16);
			if (utils.IsNumeric(sTemp))
			{
				Settings.nProcessPriority = (BYTE)atol(sTemp.c_str());
				if ((Settings.nProcessPriority < 1) || (Settings.nProcessPriority > 3))
				{
					sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be \'1\', \'2\' or \'3\'\n", sOrgLine.c_str());
					return sRet;
				}
			}
			else
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must \'1\', \'2\' or \'3\'\n", sOrgLine.c_str());
				return sRet;
			}
			continue;
		}

		if (sCurrentLine.substr(0, 9) == "timelimit")
		{
			sTemp = sCurrentLine.substr(10);
			if (utils.IsNumeric(sTemp))
			{
				Settings.iTimeLimit = atoi(sTemp.c_str());
				if (Settings.iTimeLimit != -1)
				{
					if ((Settings.iTimeLimit < 1) || (Settings.iTimeLimit > 999999))
					{
						sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nValue must be \'-1\' (no time limit) or between \'1\' and \'999999\'\n", sOrgLine.c_str());
						return sRet;
					}
				}
			}
			else
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nValue must be \'-1\' (no time limit) or between \'1\' and \'999999\'\n", sOrgLine.c_str());
				return sRet;
			}
			continue;
		}

		if (sCurrentLine.substr(0, 10) == "framerange")
		{
			sCurrentLine.erase(remove(sCurrentLine.begin(), sCurrentLine.end(), ' '), sCurrentLine.end());
			Settings.sFrameRange = sOrgLine;
			Settings.iStartFrame = 0xFFFFFFFFFF;
			Settings.iStopFrame = 0xFFFFFFFFFF;

			sTemp = sCurrentLine.substr(11);
			if (sTemp.length() < 3)
				continue;

			spos = sTemp.find(",");
			if ((spos < 1) || (spos > 27))
				continue;

			if ((utils.IsNumeric(sTemp.substr(0, spos))) && (utils.IsNumeric(sTemp.substr(spos + 1))))
			{
				Settings.iStartFrame = _atoi64(sTemp.substr(0, spos).c_str());
				Settings.iStopFrame = _atoi64(sTemp.substr(spos + 1).c_str());
			}
			continue;
		}


		iBoolValue = -1;
		if (sCurrentLine.length() > 3)
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 2) == "=0") iBoolValue = 0;
			if (sCurrentLine.substr(sCurrentLine.length() - 2) == "=1") iBoolValue = 1;
		}

		if (iBoolValue != -1)
		{
			if (sCurrentLine.substr(0, 10) == "displayfps")
				Settings.bDisplayFPS = (iBoolValue == 0) ? FALSE : TRUE;

			if (sCurrentLine.substr(0, 10) == "displaytpf")
				Settings.bDisplayTPF = (iBoolValue == 0) ? FALSE : TRUE;

			if (sCurrentLine.substr(0, 9) == "createlog")
				Settings.bCreateLog = (iBoolValue == 0) ? FALSE : TRUE;

			if (sCurrentLine.substr(0, 9) == "createcsv")
				Settings.bCreateCSV = (iBoolValue == 0) ? FALSE : TRUE;

			if (sCurrentLine.substr(0, 21) == "logfiledatetimesuffix")
				Settings.bLogFileDateTimeSuffix = (iBoolValue == 0) ? FALSE : TRUE;

			if (sCurrentLine.substr(0, 15) == "pausebeforeexit")
				Settings.bPauseBeforeExit = (iBoolValue == 0) ? FALSE : TRUE;

			if (sCurrentLine.substr(0, 17) == "invokedistributor")
				Settings.bInvokeDistributor = (iBoolValue == 0) ? FALSE : TRUE;

			if (sCurrentLine.substr(0, 20) == "allowonlyoneinstance")
				Settings.bAllowOnlyOneInstance = (iBoolValue == 0) ? FALSE : TRUE;

			if (sCurrentLine.substr(0, 14) == "monitorgpuload")
				Settings.bGPUInfo = (iBoolValue == 0) ? FALSE : TRUE;

			if (sCurrentLine.substr(0, 8) == "usecolor")
				Settings.bUseColor = (iBoolValue == 0) ? FALSE : TRUE;

			if (sCurrentLine.substr(0, 12) == "conusestdout")
				Settings.bConUseStdOut = (iBoolValue == 0) ? FALSE : TRUE;

			if (sCurrentLine.substr(0, 22) == "displayefficiencyindex")
				Settings.bDisplayEfficiencyIndex = (iBoolValue == 0) ? FALSE : TRUE;

			if (sCurrentLine.substr(0, 16) == "logestimatedtime")
				Settings.bLogEstimatedTime = (iBoolValue == 0) ? FALSE : TRUE;

			if (sCurrentLine.substr(0, 21) == "autocompleteextension")
				Settings.bAutoCompleteExtension = (iBoolValue == 0) ? FALSE : TRUE;

			if (sCurrentLine.substr(0, 20) == "logusefilesavedialog")
				Settings.bLogUseFileSaveDialog = (iBoolValue == 0) ? FALSE : TRUE;
		}
	}

	hINIFile.close();

	//Write INI file with new settings
	if (!WriteINIFile(sINIFile))
  {
		sRet = utils.StrFormat("\nCannot create \"%s\"\n", sINIFile.c_str());
		return sRet;
	}

	return "";
}


BOOL WriteINIFile(string &s_inifile)
{
	string sSettings = "";
	string sComment = "";

	ofstream hINIFile(s_inifile.c_str());
	if (!hINIFile.is_open())
		return FALSE;

	sComment =  "# This INI file can be used to set defaults for AVSMeter and/or configure\n";
	sComment += "# settings that are not implemented as command line switches.\n";
	sComment += "# See \"AVSMeter.html\" for documentation of the various settings\n";
	sComment += "# Note: If applicable, command line switches override INI file settings.\n\n";
	hINIFile << sComment;

	sSettings = utils.StrFormat("DisplayFPS=%u\n", Settings.bDisplayFPS);
	sSettings += utils.StrFormat("DisplayTPF=%u\n", Settings.bDisplayTPF);
	sSettings += utils.StrFormat("MonitorGPULoad=%u\n", Settings.bGPUInfo);
	sSettings += utils.StrFormat("DisplayEfficiencyIndex=%u\n\n", Settings.bDisplayEfficiencyIndex);

	sSettings += utils.StrFormat("TimeLimit=%I64d\n", Settings.iTimeLimit);
	sSettings += utils.StrFormat("FrameRange=%I64d,%I64d\n\n", Settings.iStartFrame, Settings.iStopFrame);

	sSettings += utils.StrFormat("CreateLog=%u\n", Settings.bCreateLog);
	sSettings += utils.StrFormat("CreateCSV=%u\n", Settings.bCreateCSV);
	sSettings += utils.StrFormat("LogDirectory=%s\n", Settings.sLogDirectory.c_str());
	sSettings += utils.StrFormat("LogFileDateTimeSuffix=%u\n", Settings.bLogFileDateTimeSuffix);
	sSettings += utils.StrFormat("LogUseFileSaveDialog=%u\n\n", Settings.bLogUseFileSaveDialog);

	sSettings += utils.StrFormat("AVSDLL=%s\n\n", Settings.sAVSDLL.c_str());

	sSettings += utils.StrFormat("AllowOnlyOneInstance=%u\n", Settings.bAllowOnlyOneInstance);
	sSettings += utils.StrFormat("ProcessPriority=%u\n", Settings.nProcessPriority);
	sSettings += utils.StrFormat("ConUseSTDOUT=%u\n", Settings.bConUseStdOut);
	sSettings += utils.StrFormat("UseColor=%u\n", Settings.bUseColor);
	sSettings += utils.StrFormat("PauseBeforeExit=%u\n\n", Settings.bPauseBeforeExit);

	sSettings += utils.StrFormat("InvokeDistributor=%u\n", Settings.bInvokeDistributor);
	sSettings += utils.StrFormat("LogEstimatedTime=%u\n", Settings.bLogEstimatedTime);
	sSettings += utils.StrFormat("AutoCompleteExtension=%u\n", Settings.bAutoCompleteExtension);

	hINIFile << sSettings;

	hINIFile.flush();
	hINIFile.close();

	return TRUE;
}


string CreateLogFile(string &s_avsfile, string &s_logbuffer, string &s_gpuinfo, vector<stPerfData> &cs_pdata, string &s_avserror, BOOL bNVVP, BOOL bOmitstPerfData)
{
	string sRet = "";
	string sAVSBuffer = "";
	string sCurrentLine = "";

	ifstream hAVSFile(s_avsfile.c_str());
	if (!hAVSFile.is_open())
	{
		sRet = utils.StrFormat("\nCannot open \"%s\"\n", s_avsfile.c_str());
		return sRet;
	}

	while (getline(hAVSFile, sCurrentLine))
		sAVSBuffer += sCurrentLine + "\n";

	hAVSFile.close();
	utils.StrTrim(sAVSBuffer);

	string sLogFile = "";
	size_t ilen = s_avsfile.length();
	ofstream hLogFile;

	if (Settings.bLogUseFileSaveDialog)
	{
		sLogFile = s_avsfile.substr(0, ilen - 4) + ".log";

		for (ilen = (sLogFile.length() - 1); ilen > 0; ilen--)
		{
			if (sLogFile[ilen] == '\\')
				break;
		}

		sLogFile = sLogFile.substr(ilen + 1);
		char szLogFile[MAX_PATH + 1] = "";
		sprintf(szLogFile, sLogFile.c_str());

		OPENFILENAME ofn;
		ofn.lStructSize       = sizeof(OPENFILENAME);
		ofn.hwndOwner         = ConsoleHWND;
		ofn.hInstance         = NULL;
		ofn.lpstrFilter       = "*.log";
		ofn.lpstrCustomFilter = NULL;
		ofn.nMaxCustFilter    = 0;
		ofn.nFilterIndex      = 1;
		ofn.lpstrFile         = szLogFile;
		ofn.nMaxFile          = MAX_PATH;
		ofn.lpstrFileTitle    = NULL;
		ofn.nMaxFileTitle     = NULL;
		ofn.lpstrInitialDir   = ".";
		ofn.lpstrTitle        = "Save log file as...";
		ofn.nFileOffset       = 0;
		ofn.nFileExtension    = 0;
		ofn.lpstrDefExt       = NULL;
		ofn.lCustData         = 0;
		ofn.lpfnHook          = NULL;
		ofn.lpTemplateName    = NULL;
		ofn.Flags             = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_EXPLORER;

		if (GetSaveFileName(&ofn) == 0)
		{
			MessageBox(GetConsoleWindow(), "\rLog file not saved\r", "AVSMeter", MB_ICONEXCLAMATION);
			return "";
		}

		hLogFile.open(szLogFile);
		if (!hLogFile.is_open())
		{
			sRet = utils.StrFormat("\nCannot create \"%s\"\n", szLogFile);
			return sRet;
		}
	}
	else
	{
		Settings.sSystemDateTime = sys.GetFormattedSystemDateTime();
		if (ilen > 4)
		{
			if (Settings.bLogFileDateTimeSuffix)
				sLogFile = s_avsfile.substr(0, ilen - 4) + " [" + Settings.sSystemDateTime + "].log";
			else
				sLogFile = s_avsfile.substr(0, ilen - 4) + ".log";
		}
		else
		{
			if (Settings.bLogFileDateTimeSuffix)
				sLogFile = s_avsfile + " [" + Settings.sSystemDateTime + "].log";
			else
				sLogFile = s_avsfile + ".log";
		}

		if (Settings.sLogDirectory != "")
		{
			size_t sLen = sLogFile.length();
			for (size_t nPos = (sLen - 1); nPos > 0; nPos--)
			{
				if (sLogFile[nPos] == '\\')
				{
					sLogFile = Settings.sLogDirectory + "\\" + sLogFile.substr(nPos + 1);
					break;
				}
			}
		}

		hLogFile.open(sLogFile.c_str());
		if (!hLogFile.is_open())
		{
			sRet = utils.StrFormat("\nCannot create \"%s\"\n", sLogFile.c_str());
			return sRet;
		}
	}

	string sLog = "";

	sLog = s_logbuffer + "\n";
	hLogFile << sLog;
	sLog = "\n[Script]\n" + sAVSBuffer + "\n\n";
	hLogFile << sLog;

	if (s_avserror != "")
		hLogFile << "\n[Errors]\n" + s_avserror + "\n\n";

	if (Settings.bGPUInfo)
		hLogFile << s_gpuinfo;

	if ((cs_pdata.size() > 0) && !bOmitstPerfData)
	{
		if (Settings.bGPUInfo)
		{
			if (bNVVP)
				sLog = "\n[Performance data]\n       Frame    Frames/sec   Time/frame(ms)   CPU(%)   GPU(%)   VPU(%)   Threads   Memory(MiB)\n";
			else
				sLog = "\n[Performance data]\n       Frame    Frames/sec   Time/frame(ms)   CPU(%)   GPU(%)   Threads   Memory(MiB)\n";
		}
		else
			sLog = "\n[Performance data]\n       Frame    Frames/sec   Time/frame(ms)   CPU(%)   Threads   Memory(MiB)\n";


		hLogFile << sLog;

		string stemp1 = "";
		string stemp2 = "";
		unsigned int uiFrame = 0;
		for (unsigned int i = 0; i < cs_pdata.size(); i++)
		{
			uiFrame = cs_pdata[i].frame + 1;
			stemp1 = utils.StrFormat("%u", uiFrame);
			string spad((12 - stemp1.length()), ' ');
			if (Settings.bGPUInfo)
			{
				if (bNVVP)
					stemp2 = utils.StrFormat("%s%u %13.3f %16.6f %8.1f %8u %8u %9u %13u", spad.c_str(), uiFrame, cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_current, cs_pdata[i].cpu_usage, cs_pdata[i].gpu_usage, cs_pdata[i].vpu_usage, cs_pdata[i].num_threads, cs_pdata[i].process_memory);
				else
					stemp2 = utils.StrFormat("%s%u %13.3f %16.6f %8.1f %8u %9u %13u", spad.c_str(), uiFrame, cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_current, cs_pdata[i].cpu_usage, cs_pdata[i].gpu_usage, cs_pdata[i].num_threads, cs_pdata[i].process_memory);
			}
			else
				stemp2 = utils.StrFormat("%s%u %13.3f %16.6f %8.1f %9u %13u", spad.c_str(), uiFrame, cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_current, cs_pdata[i].cpu_usage, cs_pdata[i].num_threads, cs_pdata[i].process_memory);

			hLogFile << stemp2 + "\n";
		}
	}

	hLogFile.flush();
	hLogFile.close();

	return sRet;
}


string CreateCSVFile(string &s_avsfile, vector<stPerfData> &cs_pdata, BOOL bNVVP)
{
	string sRet = "";

	string sCSVFile = "";
	size_t ilen = s_avsfile.length();
	ofstream hCSVFile;

	if (Settings.bLogUseFileSaveDialog)
	{
		sCSVFile = s_avsfile.substr(0, ilen - 4) + ".csv";

		for (ilen = (sCSVFile.length() - 1); ilen > 0; ilen--)
		{
			if (sCSVFile[ilen] == '\\')
				break;
		}

		sCSVFile = sCSVFile.substr(ilen + 1);
		char szCSVFile[MAX_PATH + 1] = "";
		sprintf(szCSVFile, sCSVFile.c_str());

		OPENFILENAME ofn;
		ofn.lStructSize       = sizeof(OPENFILENAME);
		ofn.hwndOwner         = ConsoleHWND;
		ofn.hInstance         = NULL;
		ofn.lpstrFilter       = "*.csv";
		ofn.lpstrCustomFilter = NULL;
		ofn.nMaxCustFilter    = 0;
		ofn.nFilterIndex      = 1;
		ofn.lpstrFile         = szCSVFile;
		ofn.nMaxFile          = MAX_PATH;
		ofn.lpstrFileTitle    = NULL;
		ofn.nMaxFileTitle     = NULL;
		ofn.lpstrInitialDir   = ".";
		ofn.lpstrTitle        = "Save CSV file as...";
		ofn.nFileOffset       = 0;
		ofn.nFileExtension    = 0;
		ofn.lpstrDefExt       = NULL;
		ofn.lCustData         = 0;
		ofn.lpfnHook          = NULL;
		ofn.lpTemplateName    = NULL;
		ofn.Flags             = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_EXPLORER;
			
		if (GetSaveFileName(&ofn) == 0)
		{
			MessageBox(GetConsoleWindow(), "\rCSV file not saved\r", "AVSMeter", MB_ICONEXCLAMATION);
			return "";
		}

		hCSVFile.open(szCSVFile);
		if (!hCSVFile.is_open())
		{
			sRet = utils.StrFormat("\nCannot create \"%s\"\n", szCSVFile);
			return sRet;
		}
	}
	else
	{
		Settings.sSystemDateTime = sys.GetFormattedSystemDateTime();
		if (ilen > 4)
		{
			if (Settings.bLogFileDateTimeSuffix)
				sCSVFile = s_avsfile.substr(0, ilen - 4) + " [" + Settings.sSystemDateTime + "].csv";
			else
				sCSVFile = s_avsfile.substr(0, ilen - 4) + ".csv";
		}
		else
		{
			if (Settings.bLogFileDateTimeSuffix)
				sCSVFile = s_avsfile + " [" + Settings.sSystemDateTime + "].csv";
			else
				sCSVFile = s_avsfile + ".csv";
		}

		if (Settings.sLogDirectory != "")
		{
			size_t sLen = sCSVFile.length();
			for (size_t nPos = (sLen - 1); nPos > 0; nPos--)
			{
				if (sCSVFile[nPos] == '\\')
				{
					sCSVFile = Settings.sLogDirectory + "\\" + sCSVFile.substr(nPos + 1);
					break;
				}
			}
		}

		hCSVFile.open(sCSVFile.c_str());
		if (!hCSVFile.is_open())
		{
			sRet = utils.StrFormat("\nCannot create \"%s\"\n", sCSVFile.c_str());
			return sRet;
		}
	}

	string sCSV = "";

	if (cs_pdata.size() > 1)
	{
		if (Settings.bGPUInfo)
		{
			if (bNVVP)
				sCSV = "Frame,Frames/sec,Frames/sec(average),Time/frame(ms),Time/frame(average)(ms),CPU(%),GPU(%),VPU(%),Threads,Memory(MiB)\n";
			else
				sCSV = "Frame,Frames/sec,Frames/sec(average),Time/frame(ms),Time/frame(average)(ms),CPU(%),GPU(%),Threads,Memory(MiB)\n";
		}
		else
			sCSV = "Frame,Frames/sec,Frames/sec(average),Time/frame(ms),Time/frame(average)(ms),CPU(%),Threads,Memory(MiB)\n";

		hCSVFile << sCSV;

		string stemp = "";
		unsigned int uiFrame = 0;
		for (unsigned int i = 0; i < cs_pdata.size(); i++)
		{
			uiFrame = cs_pdata[i].frame + 1;
			if (Settings.bGPUInfo)
			{
				if (bNVVP)
					stemp = utils.StrFormat("%u,%.3f,%.3f,%.6f,%.6f,%.1f,%u,%u,%u,%u", uiFrame, cs_pdata[i].fps_current, cs_pdata[i].fps_average, 1000.0 / cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_average, cs_pdata[i].cpu_usage, cs_pdata[i].gpu_usage, cs_pdata[i].vpu_usage, cs_pdata[i].num_threads, cs_pdata[i].process_memory);
				else
					stemp = utils.StrFormat("%u,%.3f,%.3f,%.6f,%.6f,%.1f,%u,%u,%u", uiFrame, cs_pdata[i].fps_current, cs_pdata[i].fps_average, 1000.0 / cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_average, cs_pdata[i].cpu_usage, cs_pdata[i].gpu_usage, cs_pdata[i].num_threads, cs_pdata[i].process_memory);
			}
			else
				stemp = utils.StrFormat("%u,%.3f,%.3f,%.6f,%.6f,%.1f,%u,%u", uiFrame, cs_pdata[i].fps_current, cs_pdata[i].fps_average, 1000.0 / cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_average, cs_pdata[i].cpu_usage, cs_pdata[i].num_threads, cs_pdata[i].process_memory);

			hCSVFile << stemp + "\n";
		}
	}

	hCSVFile.flush();
	hCSVFile.close();

	return sRet;
}


void PollKeys()
{
	if (Settings.bPauseBeforeExit)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\nPress any key to exit...");

		for (;;)
		{
			Sleep(100);
			if (_getch())
				break;
		}

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", Pad("").c_str());
		utils.CursorUp(1);
	}

	return;
}


void PrintConsole(BOOL bUseStdOut, WORD wAttributes, const char *fmt, ...)
{
	if (!fmt)
		return;

	va_list args;
	va_start(args, fmt);

	if (wAttributes && Settings.bUseColor)
		utils.SetConsoleColors(wAttributes);

	if (bUseStdOut)
	{
		vfprintf(stdout, fmt, args);
		fflush(stdout);
	}
	else
	{
		vfprintf(stderr, fmt, args);
		fflush(stderr);
	}

	if (wAttributes && Settings.bUseColor)
		utils.ResetConsoleColors();

	va_end(args);

	return;
}


string Pad(string s_line)
{
	int iPadLen = utils.GetConsoleWidth() - (int)s_line.length() - 1;
	if (iPadLen > 0)
		s_line.append(iPadLen, ' ');

	return s_line;
}


unsigned int CalculateFrameInterval(string &s_avsfile, string &s_error)
{
	s_error = "";
	unsigned int uiFrameInterval = 10;

	HINSTANCE hDLL;
	if (Settings.sAVSDLL == "")
		hDLL = ::LoadLibrary("avisynth");
	else
		hDLL = ::LoadLibrary(Settings.sAVSDLL.c_str());

	if (!hDLL)
	{
		s_error = "Cannot load avisynth.dll";
		return uiFrameInterval;
	}

	IScriptEnvironment *AVS_env = 0;

	try
	{
		_set_se_translator(SE_Translator);

		CREATE_ENV *CreateEnvironment = (CREATE_ENV *)GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nFailed to load CreateScriptEnvironment()\n");
			PollKeys();
			return -1;
		}

		AVS_env = CreateEnvironment(AvisynthInfo.iInterfaceVersion);

		if (!AVS_env)
		{
			s_error = "Could not create IScriptenvironment";
			::FreeLibrary(hDLL);
			return uiFrameInterval;
		}

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("Loading script...").c_str());

		AVS_linkage = AVS_env->GetAVSLinkage();
		AVSValue AVS_main;
		AVSValue AVS_temp;
		PClip AVS_clip;
		VideoInfo	AVS_vidinfo;

		AVS_main = AVS_env->Invoke("Import", s_avsfile.c_str());

		if (!AVS_main.IsClip()) //not a clip
			AVS_env->ThrowError("Script did not return a video clip:\n%s", s_avsfile.c_str());

		int iMTMode = 0;
		try
		{
			AVS_temp = AVS_env->Invoke("GetMTMode", false);
			iMTMode = AVS_temp.IsInt() ? AVS_temp.AsInt() : 0;
			if ((iMTMode > 0) && (iMTMode < 5) && Settings.bInvokeDistributor)
				AVS_main = AVS_env->Invoke("Distributor", AVS_main);
		}
		catch (IScriptEnvironment::NotFound)
		{
			iMTMode = 0;
		}

		AVS_clip = AVS_main.AsClip();
		AVS_vidinfo = AVS_clip->GetVideoInfo();

		unsigned int uiTotalFrames = (unsigned int)AVS_vidinfo.num_frames;

		if (!AVS_vidinfo.HasVideo())
			AVS_env->ThrowError("Script did not return a video clip:\n%s", s_avsfile.c_str());

		unsigned int uiFrame = 0;
		double TDelta = 0.0;
		double T0 = timer.GetSTDTimer();
		unsigned __int64 D0 = 0;
		while ((TDelta < 5.0) && (uiFrame < uiTotalFrames))
		{
			if (_kbhit())
			{
				if (_getch() == 0x1B) //ESC
					AVS_env->ThrowError("\'ESC\' pressed, cancelled.");
			}

			if ((timer.GetSTDTimerMS() - D0) >= 150)
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\rPre-scanning script (F%u / T%.2lf)\r", uiFrame, TDelta);
				D0 = timer.GetSTDTimerMS();
			}

			PVideoFrame src_frame = AVS_clip->GetFrame(uiFrame, AVS_env);
			++uiFrame;
			TDelta = timer.GetSTDTimer() - T0;
		}

		double dAverageFrameTime = 0.0;
		if (TDelta >= ((double)MIN_RUNTIME / 1000.0))
			dAverageFrameTime = (1000.0 * TDelta) / (double)uiFrame; //convert to milliseconds
		else
			AVS_env->ThrowError("Script runtime is too short for meaningful measurements");

		uiFrameInterval = 1000;
		if (dAverageFrameTime > (MIN_TIME_PER_FRAMEINTERVAL /  500.0)) uiFrameInterval =  500;
		if (dAverageFrameTime > (MIN_TIME_PER_FRAMEINTERVAL /  200.0)) uiFrameInterval =  200;
		if (dAverageFrameTime > (MIN_TIME_PER_FRAMEINTERVAL /  100.0)) uiFrameInterval =  100;
		if (dAverageFrameTime > (MIN_TIME_PER_FRAMEINTERVAL /   50.0)) uiFrameInterval =   50;
		if (dAverageFrameTime > (MIN_TIME_PER_FRAMEINTERVAL /   20.0)) uiFrameInterval =   20;
		if (dAverageFrameTime > (MIN_TIME_PER_FRAMEINTERVAL /   10.0)) uiFrameInterval =   10;
		if (dAverageFrameTime > (MIN_TIME_PER_FRAMEINTERVAL))          uiFrameInterval =    1;

		AVS_clip = 0;
		AVS_main = 0;
		AVS_temp = 0;
		Sleep(DSE_DELAY);
		AVS_env->DeleteScriptEnvironment();
		AVS_env = 0;
		AVS_linkage = 0;

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
	}
	catch (AvisynthError err)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
		s_error = utils.StrFormat("%s", (PCSTR)err.msg);
		uiFrameInterval = 10;
	}
	catch (exception& ex)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
		s_error = ex.what();
		uiFrameInterval = 10;
	}
	catch (...)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
		s_error = utils.SysErrorMessage();
		uiFrameInterval = 10;
	}

	if (!::FreeLibrary(hDLL))
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
		s_error = "Cannot unload avisynth.dll";
		uiFrameInterval = 10;
	}

	return uiFrameInterval;
}


void PrintUsage()
{
	PrintConsole(TRUE, BG_BLACK | FG_HYELLOW, "\nUsage 1:  AVSMeter script.avs [switches]\n\n");

	PrintConsole(TRUE, COLOR_EMPHASIS, "Switches:\n");
	
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -avsdll             Specify avisynth.dll to be used\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -info   [-i]        Display clip info\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -log    [-l]        Create log file\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -csv                Create csv file\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -gpu                Display GPU/VPU usage (requires GPU-Z)\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -range=first,last   Set frame range\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -timelimit=n        Set time limit (seconds)\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -priority=n         Set process priority (1:low, 2:normal, 3:high)\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -o                  Omit script pre-scanning\n\n\n");


	PrintConsole(TRUE, BG_BLACK | FG_HYELLOW, "\nUsage 2:  AVSMeter avsinfo [switches]\n\n");

	PrintConsole(TRUE, COLOR_EMPHASIS, "Switches:\n");

 	PrintConsole(TRUE, COLOR_EMPHASIS, "  -avsdll             Specify avisynth.dll to be used\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -c                  Specify custom plugin directory\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -log    [-l]        Create log file\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -lf                 Add internal/external functions to the log file\n\n\n\n");


	PrintConsole(TRUE, COLOR_EMPHASIS, "  For more info on the command line switches and INI file\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  settings read the documentation (AVSMeter.html) included\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  in the distribution package.\n");

	PrintConsole(TRUE, COLOR_DEFAULT, "\n");
	return;
}

