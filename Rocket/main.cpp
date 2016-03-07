// For conditions of distribution and use, see copyright notice in LICENSE

#include "CoreDefines.h"
#include "Framework.h"
#include "TundraCoreApi.h"
#include "Win.h"

#if defined (WIN32) && defined(ROCKET_CRASH_REPORTER_ENABLED)
#include "Application.h"
#include "CrashRpt.h"
#include <tchar.h>
#endif

int TUNDRACORE_API run(int argc, char **argv);

#ifdef ANDROID
#include "StaticPluginRegistry.h"
/// \todo Eliminate the need to list static plugins explicitly here
REGISTER_STATIC_PLUGIN(OgreRenderingModule)
REGISTER_STATIC_PLUGIN(PhysicsModule)
REGISTER_STATIC_PLUGIN(EnvironmentModule)
REGISTER_STATIC_PLUGIN(TundraLogicModule)
REGISTER_STATIC_PLUGIN(AssetModule)
REGISTER_STATIC_PLUGIN(JavascriptModule)
REGISTER_STATIC_PLUGIN(AvatarModule)
REGISTER_STATIC_PLUGIN(DebugStatsModule)
#endif

#if defined(_WIN64) && defined(_DEBUG) 
#include <kNet/64BitAllocDebugger.h>
BottomMemoryAllocator bma; // Use kNet's BottomMemoryAllocator on to catch potential 64-bit memory allocation bugs early on.
#endif

#if defined(_MSC_VER) // Windows application entry point.
int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR lpCmdLine, int /*nShowCmd*/)
{
#if defined (WIN32) && defined(ROCKET_CRASH_REPORTER_ENABLED)
    WCHAR szAppName[MAX_PATH];
    WCHAR szAppVersion[MAX_PATH];
    WCHAR szDataDir[MAX_PATH];
    WCHAR szLangFile[MAX_PATH];
    WCHAR szIcon[MAX_PATH];

    MultiByteToWideChar(CP_ACP, 0, QString("%1 %2").arg(Application::OrganizationName()).arg(Application::ApplicationName()).toStdString().c_str(), -1, szAppName, NUMELEMS(szAppName));
    MultiByteToWideChar(CP_ACP, 0, Application::Version(), -1, szAppVersion, NUMELEMS(szAppVersion));
    MultiByteToWideChar(CP_ACP, 0, QString(Application::UserDataDirectory() + "crash-reporter").toStdString().c_str(), -1, szDataDir, NUMELEMS(szDataDir));
    MultiByteToWideChar(CP_ACP, 0, QString(Application::InstallationDirectory() + "data\\crash-reporter\\crashrpt_lang_EN.ini").toStdString().c_str(), -1, szLangFile, NUMELEMS(szLangFile));
    MultiByteToWideChar(CP_ACP, 0, QString(Application::InstallationDirectory() + "Rocket.exe,0").toStdString().c_str(), -1, szIcon, NUMELEMS(szIcon));

    CR_INSTALL_INFO info;
    memset(&info, 0, sizeof(CR_INSTALL_INFO));
    info.cb = sizeof(CR_INSTALL_INFO);
    info.pszAppName = szAppName;
    info.pszAppVersion = szAppVersion;
    info.pszErrorReportSaveDir = szDataDir;
    info.pszLangFilePath = szLangFile;
    info.pszCustomSenderIcon = szIcon;

    /// @todo Add proper domain here once we setup the server!
    info.pszUrl = _T("http://127.0.0.1/crashfix/index.php/crashReport/uploadExternal"); // URL for sending reports over HTTP.
    info.uPriorities[CR_HTTP]  = 1;
    info.uPriorities[CR_SMTP]  = CR_NEGATIVE_PRIORITY; // Don't use SMTP
    info.uPriorities[CR_SMAPI] = CR_NEGATIVE_PRIORITY; // Don't use Simple MAPI
    info.dwFlags = CR_INST_ALL_POSSIBLE_HANDLERS | CR_INST_APP_RESTART;

    CrAutoInstallHelper crHelper(&info);
    if (crHelper.m_nInstallStatus != 0)
    {
        TCHAR buff[256];
        crGetLastErrorMsg(buff, 256);
        _tprintf(_T("%s\n"), buff);
        return 1;
    }
#endif

    std::string cmdLine(lpCmdLine);

    // Parse the Windows command line.
    std::vector<std::string> arguments;
    unsigned i;
    unsigned cmdStart = 0;
    unsigned cmdEnd = 0;
    bool cmd = false;
    bool quote = false;

    // Inject executable name as Framework will expect it to be there.
    // Otherwise the first param will be ignored (it assumes its the executable name).
    // In WinMain() its not included in the 'lpCmdLine' param.
    arguments.push_back("Tundra.exe");
    arguments.push_back("--config");
    arguments.push_back("rocket.json");
    for(i = 0; i < cmdLine.length(); ++i)
    {
        if (cmdLine[i] == '\"')
            quote = !quote;
        if ((cmdLine[i] == ' ') && (!quote))
        {
            if (cmd)
            {
                cmd = false;
                cmdEnd = i;
                arguments.push_back(cmdLine.substr(cmdStart, cmdEnd-cmdStart));
            }
        }
        else
        {
            if (!cmd)
            {
               cmd = true;
               cmdStart = i;
            }
        }
    }
    if (cmd)
        arguments.push_back(cmdLine.substr(cmdStart, i-cmdStart));

    std::vector<const char*> argv;
    for(size_t i = 0; i < arguments.size(); ++i)
        argv.push_back(arguments[i].c_str());

    return run(!argv.empty() ? (int)argv.size() : 0, !argv.empty() ? (char**)&argv[0] : 0);
}
#else // Unix entry point
int main(int argc, char **argv)
{
    // Rocket executable: inject viewer-admino.xml as config
    std::vector<const char*> args;
    for(int i = 0; i < argc; ++i)
        args.push_back(argv[i]);

    args.push_back("--config");
    args.push_back("rocket.json");
    return run(args.size(), (char**)&args[0]);
}
#endif
