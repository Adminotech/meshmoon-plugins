// For conditions of distribution and use, see copyright notice in LICENSE

#include "TundraCoreApi.h"
#include "CoreDefines.h"

#if defined (WIN32) && defined(ROCKET_CRASH_REPORTER_ENABLED)
#include "Application.h"
#include "Win.h"
#include "CrashRpt.h"
#include <tchar.h>
#endif

#include <vector>

int TUNDRACORE_API run(int argc, char **argv);

#if defined(_WIN64) && defined(_DEBUG)
#include <kNet/64BitAllocDebugger.h>
BottomMemoryAllocator bma; // Use kNet's BottomMemoryAllocator on to catch potential 64-bit memory allocation bugs early on.
#endif

int main(int argc, char **argv)
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

    // First push the executable then rocket config
    // and after it the extra params.
    std::vector<const char*> args;
    if (argc > 0)
        args.push_back(argv[0]);
    args.push_back("--config");
    args.push_back("rocket.json");
    for(int i = 1; i < argc; ++i)
        args.push_back(argv[i]);

    return run((int)args.size(), (char**)&args[0]);   
}
