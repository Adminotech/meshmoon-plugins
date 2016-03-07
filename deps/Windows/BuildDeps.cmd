@echo off
echo.

:: Enable the delayed environment variable expansion needed in VSConfig.cmd.
setlocal EnableDelayedExpansion

set TUNDRA_ROOT=%CD%\..\..\..\..

:: Make sure we're running in Visual Studio Command Prompt
IF "%VSINSTALLDIR%"=="" (
   "%TUNDRA_ROOT%\tools\Windows\Utils\cecho" {0C}Batch file not executed from Visual Studio Command Prompt - cannot proceed!{# #}{\n}
   GOTO :ERROR
)

:: cd to <TUNDRA_ROOT>\tools\Windows so that VSConfig.cmd works properly
set ORIG_DIR=%CD%
cd "%TUNDRA_ROOT%\tools\Windows"
call VSConfig.cmd %1
cd %ORIG_DIR%\..

:: Override DEPS set in VSConfig.cmd
set TUNDRA_DEPS=%DEPS%
set DEPS=%CD%

set QTDIR=%TUNDRA_DEPS%\qt

:: Set HOME so msysgit will find ssh credentials
:: for cloning private repos.
set HOME=%USERPROFILE%

:: CEF
SET CEF_VERSION="cef_binary_1.1180.832_windows"
if %VS_VER%==vs2008 (
    IF NOT EXIST "%DEPS%\cef". (
        cd "%DEPS%"
        IF NOT EXIST "%CEF_VERSION%.zip". (
            cecho {0D}Downloading CEF %CEF_VERSION%{# #}{\n}
            wget http://chromiumembedded.googlecode.com/files/%CEF_VERSION%.zip
            IF NOT %ERRORLEVEL%==0 GOTO :ERROR
        )
        7za x -y "%CEF_VERSION%.zip"
        ren %CEF_VERSION% cef
    )

    cd "%DEPS%\cef"
    IF NOT EXIST "Release\lib\libcef_dll_wrapper.lib". (
        sed -e s/"RuntimeLibrary=\"0\""/"RuntimeLibrary=\"2\""/g -e s/"WarnAsError=\"true\""/"WarnAsError=\"false\""/g <libcef_dll_wrapper.%VCPROJ_FILE_EXT% >libcef_dll_wrapper_dll_runtime_release.%VCPROJ_FILE_EXT%
        cecho {0D}Building CEF Release...{# #}{\n}
        MSBuild libcef_dll_wrapper_dll_runtime_release.%VCPROJ_FILE_EXT% /p:configuration=Release /nologo
        IF EXIST "%TUNDRA_BIN%\libcef.dll". del "%TUNDRA_BIN%\libcef.dll"
        :: VS >= 2010 seems to have different semantics for ProjectName so the output file is named differently.
        IF NOT %VS_VER%==vs2008 (
            cd Release\lib\
            ren libcef_dll_wrapper_dll_runtime_release.lib libcef_dll_wrapper.lib
            cd ..\..
        )
    )
    IF NOT EXIST "Debug\lib\libcef_dll_wrapper.lib". (
        sed -e s/"RuntimeLibrary=\"1\""/"RuntimeLibrary=\"3\""/g -e s/"WarnAsError=\"true\""/"WarnAsError=\"false\""/g <libcef_dll_wrapper.%VCPROJ_FILE_EXT% >libcef_dll_wrapper_dll_runtime_debug.%VCPROJ_FILE_EXT%
        cecho {0D}Building CEF Debug...{# #}{\n}
        MSBuild libcef_dll_wrapper_dll_runtime_debug.%VCPROJ_FILE_EXT% /p:configuration=Debug /nologo
        IF EXIST "%TUNDRA_BIN%\libcef.dll". del "%TUNDRA_BIN%\libcef.dll"
        :: VS >= 2010 seems to have different semantics for ProjectName so the output file is named differently.
        IF NOT %VS_VER%==vs2008 (
            cd Debug\lib\
            ren libcef_dll_wrapper_dll_runtime_debug.lib libcef_dll_wrapper.lib
            cd ..\..
        )
    )

    cecho {0D}Deploying CEF to %TUNDRA_BIN%{# #}{\n}
    copy /Y "%DEPS%\cef\Release\libcef.dll" "%TUNDRA_BIN%"
    copy /Y "%DEPS%\cef\Release\icudt.dll" "%TUNDRA_BIN%"
    copy /Y "%DEPS%\cef\Release\libEGL.dll" "%TUNDRA_BIN%"
    copy /Y "%DEPS%\cef\Release\libGLESv2.dll" "%TUNDRA_BIN%"
    xcopy /Q /E /I /C /H /R /Y "%DEPS%\cef\Release\locales\*" "%TUNDRA_BIN%\data\cef\locales"
    copy /Y "%DEPS%\cef\Release\*.pak" "%TUNDRA_BIN%\data\cef"

) ELSE (
    cecho {0D}Skipping CEF for Visual Studio 2010{# #}{\n}
    cecho {0A}-- Run VS2008/BuildDeps.cmd to build 32bit VC9 CEF{# #}{\n}
    cecho {0A}-- Run VS2008/BuildQt.cmd   to build 32bit VC9 Qt{# #}{\n}
)

:: qts3
IF NOT EXIST "%DEPS%\qts3". (
    cd "%DEPS%"
    cecho {0D}Cloning qts3{# #}{\n}
    call git clone git@github.com:Adminotech/qts3.git
)
IF NOT EXIST "%DEPS%\qts3\qts3.sln". (
    cd "%DEPS%\qts3"
    cecho {0D}Running qts3 cmake{# #}{\n}
    cmake -G %GENERATOR%

    cecho {0D}Building qts3 Debug...{# #}{\n}
    MSBuild qts3.sln /p:configuration=Debug /nologo /m:%NUMBER_OF_PROCESSORS%
    cecho {0D}Building qts3 Release...{# #}{\n}
    MSBuild qts3.sln /p:configuration=Release /nologo /m:%NUMBER_OF_PROCESSORS%
    cecho {0D}Building qts3 RelWithDebInfo...{# #}{\n}
    MSBuild qts3.sln /p:configuration=RelWithDebInfo /nologo /m:%NUMBER_OF_PROCESSORS%
)

:: crashrpt
:: set CRASHRPT_VERSION="v.1.4.2_r1609"
:: set CRASHRPT_LIB="CrashRpt1402"
:: set CRASHRPT_SENDER="CrashSender1402"
:: IF NOT EXIST "%DEPS%\crashrpt". (
::     cd "%DEPS%"
::     IF NOT EXIST "CrashRpt_%CRASHRPT_VERSION%.7z". (
::         cecho {0D}Downloading crashrpt %CRASHRPT_VERSION%{# #}{\n}
::         wget http://crashrpt.googlecode.com/files/CrashRpt_%CRASHRPT_VERSION%.7z
::         IF NOT %ERRORLEVEL%==0 GOTO :ERROR
::     )
::     7za x -y -ocrashrpt "CrashRpt_%CRASHRPT_VERSION%.7z"
:: )
:: 
:: IF NOT EXIST "%DEPS%\crashrpt\CrashRpt.sln". (
::     cd "%DEPS%\crashrpt"
::     cecho {0D}Running crashrpt cmake{# #}{\n}
::     cmake -G %GENERATOR%
:: 
::     cecho {0D}Building crashrpt Debug{# #}{\n}
::     MSBuild CrashRpt.sln /p:configuration=Debug /nologo /clp:ErrorsOnly /m:%NUMBER_OF_PROCESSORS%
::     cecho {0D}Building crashrpt Release{# #}{\n}
::     MSBuild CrashRpt.sln /p:configuration=Release /nologo /clp:ErrorsOnly /m:%NUMBER_OF_PROCESSORS%
:: 
::     IF %VS_PLATFORM%==x64 (
::         xcopy /Q /E /I /C /H /R /Y bin\x64\* bin\
::         xcopy /Q /E /I /C /H /R /Y lib\x64\* lib\
::     )
:: )

:: cecho {0D}Deploying crashrpt to %TUNDRA_BIN%{# #}{\n}
:: copy /Y "%DEPS%\crashrpt\bin\%CRASHRPT_LIB%.dll" "%TUNDRA_BIN%"
:: copy /Y "%DEPS%\crashrpt\bin\%CRASHRPT_LIB%d.dll" "%TUNDRA_BIN%"
:: copy /Y "%DEPS%\crashrpt\bin\%CRASHRPT_SENDER%.exe" "%TUNDRA_BIN%"
:: copy /Y "%DEPS%\crashrpt\bin\%CRASHRPT_SENDER%d.exe" "%TUNDRA_BIN%"
:: IF NOT EXIST "%TUNDRA_BIN%\data\crash-reporter". mkdir "%TUNDRA_BIN%\data\crash-reporter"
:: copy /Y "%DEPS%\crashrpt\lang_files\crashrpt_lang_EN.ini" "%TUNDRA_BIN%\data\crash-reporter\crashrpt_lang_EN.ini"

:: OpenNI. Get it from http://www.openni.org/openni-sdk and install to the default location.
:: cecho {0D}Skipping OpenNI, get it from http://www.openni.org/openni-sdk{# #}{\n}

:: Triton and SilverLining
cecho {0D}Skipping Triton and SilverLining SDKs, get them from http://sundog-soft.com/sds/evaluate/download/{# #}{\n}

:: Oculus
cecho {0D}Skipping Oculus SDK, get it from https://developer.oculusvr.com/ and make OCULUS_SDK environment variable to point to it.{# #}{\n}

:: Visual Studio 2010 specific
set VLC_VERSION=2.0.1
if %VS_VER%==vs2010 (
    :: VLC
    IF NOT EXIST "%DEPS%\vlc-%VLC_VERSION%-win32.zip". (
        CD "%DEPS%"
        rmdir /S /Q "%DEPS%\vlc"
        cecho {0D}Downloading VLC %VLC_VERSION%{# #}{\n}
        wget --no-check-certificate http://sourceforge.net/projects/vlc/files/%VLC_VERSION%/win32/vlc-%VLC_VERSION%-win32.zip/download
        IF NOT EXIST "%DEPS%\vlc-%VLC_VERSION%-win32.zip". GOTO :ERROR
    )

    IF NOT EXIST "%DEPS%\vlc". (
        CD "%DEPS%"
        mkdir vlc
        cecho {0D}Extracting VLC %VLC_VERSION% package to "%DEPS%\vlc\vlc-%VLC_VERSION%"{# #}{\n}
        7za x -y -ovlc vlc-%VLC_VERSION%-win32.zip
        cd vlc
        IF NOT %ERRORLEVEL%==0 GOTO :ERROR
        mkdir lib
        mkdir include
        mkdir bin\plugins\vlcplugins
        IF NOT %ERRORLEVEL%==0 GOTO :ERROR
        :: Copy from extracted location to our subfolders
        cecho {0D}Copying needed VLC %VLC_VERSION% files to \bin \lib and \include{# #}{\n}
        copy /Y vlc-%VLC_VERSION%\*.dll bin\
        xcopy /E /I /C /H /R /Y vlc-%VLC_VERSION%\plugins\*.* bin\plugins\vlcplugins
        xcopy /E /I /C /H /R /Y vlc-%VLC_VERSION%\sdk\include\*.* include
        copy /Y vlc-%VLC_VERSION%\sdk\lib\*.lib lib\
        :: Remove extracted folder, not needed anymore
        rmdir /S /Q vlc-%VLC_VERSION%
        IF NOT %ERRORLEVEL%==0 GOTO :ERROR
        :: Force deployment and clean vlc plugins cache file
        del /Q "%TUNDRA_BIN%\libvlc.dll"
        rmdir /S /Q "%TUNDRA_BIN%\plugins\vlcplugins"
        del /Q "%TUNDRA_BIN%\plugins\plugins*.dat"
        IF NOT %ERRORLEVEL%==0 GOTO :ERROR
    )

    cecho {0D}Deploying VLC %VLC_VERSION% to %TUNDRA_BIN%{# #}{\n}
    xcopy /Q /E /I /C /H /R /Y "%DEPS%\vlc\bin\*.*" "%TUNDRA_BIN%"
    IF NOT %ERRORLEVEL%==0 GOTO :ERROR

    set ORIG_PATH="%CD%"

    :: RocketWebBrowser
    cd "%DEPS%\..\MeshmoonComponents\rocketwebbrowser"
    cecho {0D}Building RocketWebBrowser Release{# #}{\n}
    MSBuild RocketWebBrowser_Manual.vcxproj /p:configuration="Release" /p:platform="Win32" /nologo /m:%NUMBER_OF_PROCESSORS%
    cecho {0D}Building RocketWebBrowser RelWithDebInfo{# #}{\n}
    MSBuild RocketWebBrowser_Manual.vcxproj /p:configuration="RelWithDebInfo" /p:platform="Win32" /nologo /m:%NUMBER_OF_PROCESSORS%
    cecho {0D}Building RocketWebBrowser Debug{# #}{\n}
    MSBuild RocketWebBrowser_Manual.vcxproj /p:configuration="Debug" /p:platform="Win32" /nologo /m:%NUMBER_OF_PROCESSORS%
    cecho {0D}Deploying RocketWebBrowser to %TUNDRA_BIN%{# #}{\n}
    copy /Y "Release\RocketWebBrowser_Manual.exe" "%TUNDRA_BIN%\RocketWebBrowser.exe"
    copy /Y "Debug\RocketWebBrowser_Manuald.exe" "%TUNDRA_BIN%\RocketWebBrowserd.exe"

    :: RocketMediaPlayer
    cd "%DEPS%\..\MeshmoonComponents\rocketmediaplayer"
    cecho {0D}Building RocketMediaPlayer Release{# #}{\n}
    MSBuild RocketMediaPlayer_Manual.vcxproj /p:configuration="Release" /p:platform="Win32" /nologo /m:%NUMBER_OF_PROCESSORS%
    cecho {0D}Building RocketMediaPlayer RelWithDebInfo{# #}{\n}
    MSBuild RocketMediaPlayer_Manual.vcxproj /p:configuration="RelWithDebInfo" /p:platform="Win32" /nologo /m:%NUMBER_OF_PROCESSORS%
    cecho {0D}Building RocketMediaPlayer Debug{# #}{\n}
    MSBuild RocketMediaPlayer_Manual.vcxproj /p:configuration="Debug" /p:platform="Win32" /nologo /m:%NUMBER_OF_PROCESSORS%
    cecho {0D}Deploying RocketMediaPlayer to %TUNDRA_BIN%{# #}{\n}
    copy /Y "Release\RocketMediaPlayer_Manual.exe" "%TUNDRA_BIN%\RocketMediaPlayer.exe"
    copy /Y "Debug\RocketMediaPlayer_Manuald.exe" "%TUNDRA_BIN%\RocketMediaPlayerd.exe"
    
    cd "%ORIG_PATH%"
)

echo.
cecho {0A}Meshmoon Rocket dependencies built.{# #}{\n}
set PATH=%ORIGINAL_PATH%
cd %DEPS%
GOTO :EOF

:ERROR
echo.
..\..\..\..\tools\Windows\utils\cecho {0C}An error occurred! Aborting!{# #}{\n}
set PATH=%ORIGINAL_PATH%
cd %DEPS%
pause

endlocal
