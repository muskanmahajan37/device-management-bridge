/*
Copyright 2018 Microsoft
Permission is hereby granted, free of charge, to any person obtaining a copy of this software
and associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "stdafx.h"
#include "ConfigUtils.h"
#include "DMBridgeService.h"
#include "RegistryUtils.h"
#include "Logger.h"
#include "Constants.h"

#include "NTService.h"

#define SERVICE_NAME             L"SystemConfiguratorBridge"
#define SERVICE_DISPLAY_NAME     L"System Configurator Bridge"
#define SERVICE_START_TYPE       SERVICE_DEMAND_START
#define SERVICE_DEPENDENCIES     L""
#define SERVICE_ACCOUNT          L"NT AUTHORITY\\SYSTEM"
#define SERVICE_PASSWORD         L""

constexpr char* NtserviceConfigKey = "servicemanager";

using namespace std;

void PrintUsage();
void LoadConfiguration();

int wmain(int argc, wchar_t *argv[])
{
	//
	// HKLM\\Software\\Microsoft\\IoTDMBridge
	//   DebugLogFile
	//
	wstring logFileName;
	if (Utils::TryReadRegistryValue(IoTDMRegistryRoot, RegDebugLogFile, logFileName) == ERROR_SUCCESS)
	{
		if (logFileName.length() > 0)
		{
			gLogger.SetLogFileName(logFileName);
		}
	}

	TRACE("Entering wmain...");
	LoadConfiguration();

	if ((argc > 1) && ((*argv[1] == L'-' || (*argv[1] == L'/'))))
	{
		if (_wcsicmp(L"install", argv[1] + 1) == 0)
		{
			DMBridgeService::Install(
				SERVICE_NAME,               // Name of service
				SERVICE_DISPLAY_NAME,       // Name to display
				SERVICE_START_TYPE,         // Service start type
				SERVICE_DEPENDENCIES,       // Dependencies
				SERVICE_ACCOUNT,            // Service running account
				SERVICE_PASSWORD            // Password of the account
			);
			return 0;
		}
		else if (_wcsicmp(L"uninstall", argv[1] + 1) == 0)
		{
			DMBridgeService::Uninstall(SERVICE_NAME);
			return 0;
		}
		else if (_wcsicmp(L"console", argv[1] + 1) == 0)
		{
			TRACE("Directly starting DMBridgeServer");
			DMBridgeServer::Setup();
			DMBridgeServer::Listen();
			return 0;
		}
		else if (_wcsicmp(L"help", argv[1] + 1) == 0)
		{
			PrintUsage();
			return 0;
		}
		else if (argc > 2 && _wcsicmp(L"logging", argv[1] + 1) == 0)
		{
			if (argc > 3 && _wcsicmp(L"enable", argv[2]) == 0)
			{
				logFileName = argv[3];
				Utils::WriteRegistryValue(IoTDMRegistryRoot, RegDebugLogFile, logFileName);
				TRACEP(L"Enabled file logging to: ", logFileName);
				return 0;
			}
			else if (_wcsicmp(L"disable", argv[2]) == 0)
			{
				logFileName = L"";
				Utils::WriteRegistryValue(IoTDMRegistryRoot, RegDebugLogFile, logFileName);
				TRACE("Disabled file logging");
				return 0;
			}
			else if (_wcsicmp(L"state", argv[2]) == 0)
			{
				if (logFileName.length() == 0)
				{
					TRACE("File logging is disabled");
				}
				else
				{
					TRACEP(L"Logging to: ", logFileName);
				}
				return 0;
			}
		}
		else if (argc > 2 && _wcsicmp(L"config", argv[1] + 1) == 0)
		{
			wstring configFileName;
			if (argc > 3 && _wcsicmp(L"set", argv[2]) == 0)
			{
				configFileName = argv[3];
				Utils::WriteRegistryValue(IoTDMRegistryRoot, RegConfigFile, configFileName);
				TRACEP(L"Set config file to: ", configFileName);
				return 0;
			}
			else if (_wcsicmp(L"default", argv[2]) == 0)
			{
				configFileName = L"";
				Utils::WriteRegistryValue(IoTDMRegistryRoot, RegConfigFile, configFileName);
				TRACE("Reset config file path to default");
				return 0;
			}
			else if (_wcsicmp(L"state", argv[2]) == 0)
			{
				if (Utils::TryReadRegistryValue(IoTDMRegistryRoot, RegConfigFile, configFileName) == ERROR_SUCCESS)
				{
					if (configFileName.length() > 0)
					{
						TRACEP(L"Using config file: ", configFileName);
						return 0;
					}
				}
				TRACEP(L"Using default config file path: ./", DefaultConfigFile);
				return 0;
			}
		}

	}
	else
	{
		PrintUsage();

		TRACE(L"Running service...");
		DMBridgeService service(SERVICE_NAME);
		DMBridgeService::Run(service);
		return 0;
	}
	PrintUsage();
	return 0;
}

void PrintUsage()
{
	TRACE(L"Usage:");
	TRACE(L" -help      to print this usage message.");
	TRACE(L" -install   to install the service.");
	TRACE(L" -uninstall to remove the service.");
	TRACE(L" -console   to run the service as the current account.");
	TRACE(L" -logging enable {filepath} to turn on file logging at the specified path.");
	TRACE(L" -logging disable           to turn off file logging.");
	TRACE(L" -logging state             to return the current file log, if any.");
	TRACE(L" -config set {filepath}		to set the file path of the configuration file.");
	TRACE(L" -config default			to reset the configuration file path to default.");
	TRACE(L" -config state				to return the current configuration file path.");
	TRACE(L"");
}

void LoadConfiguration()
{
	TRACE(__FUNCTION__);
	Json::Value root = ConfigUtils::LoadConfigFile();

	unique_ptr<DMBridgeConfig> dmConfig = make_unique<DMBridgeConfig>(root);
	unique_ptr<NTServiceConfig> ntConfig = make_unique<NTServiceConfig>(
		ConfigUtils::SafelyGetSection(root, NtserviceConfigKey));

	DMBridgeServer::ApplyConfig(dmConfig);
	NTService::ApplyConfig(ntConfig);
	TRACE("------ Done applying configurations ------");
}