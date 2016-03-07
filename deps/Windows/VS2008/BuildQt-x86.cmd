@echo off
echo.

:: Enable the delayed environment variable expansion needed in VSConfig.cmd.
setlocal EnableDelayedExpansion

cd..

set TUNDRA_ROOT=%CD%\..\..\..\..

:: Make sure we're running in Visual Studio Command Prompt
IF "%VSINSTALLDIR%"=="" (
   "%TUNDRA_ROOT%\tools\Windows\Utils\cecho" {0C}Batch file not executed from Visual Studio Command Prompt - cannot proceed!{# #}{\n}
   GOTO :ERROR
)

:: cd to <TUNDRA_ROOT>\tools\Windows so that VSConfig.cmd works properly
set ORIG_DIR=%CD%
cd "%TUNDRA_ROOT%\tools\Windows"
call VSConfig.cmd "Visual Studio 9 2008"
cd %ORIG_DIR%\..

:: Override DEPS set in VSConfig.cmd
set DEPS=%CD%

:: Build Qt, only QtCore and QtNetwork (and some extra cruft we cannot seem to disable) are built, without OpenSSL support.
set QT_VER=4.8.6
set QT_URL=http://download.qt-project.org/archive/qt/4.8/%QT_VER%/qt-everywhere-opensource-src-%QT_VER%.zip
IF NOT EXIST "%DEPS%\qt". (
    cd "%DEPS%"
    IF NOT EXIST qt-everywhere-opensource-src-%QT_VER%.zip. (
        cecho {0D}Downloading Qt %QT_VER%. Please be patient, this will take a while.{# #}{\n}
        wget %QT_URL%
        IF NOT %ERRORLEVEL%==0 GOTO :ERROR
    )

    cecho {0D}Extracting Qt %QT_VER% sources to "%DEPS%\qt".{# #}{\n}
    mkdir qt
    7za x -y -oqt qt-everywhere-opensource-src-%QT_VER%.zip
    IF NOT %ERRORLEVEL%==0 GOTO :ERROR
    cd qt
    ren qt-everywhere-opensource-src-%QT_VER% qt-src-%QT_VER%
    IF NOT EXIST "%DEPS%\qt" GOTO :ERROR
) ELSE (
    cecho {0D}Qt %QT_VER% already downloaded. Skipping.{# #}{\n}
)

set JOM_VERSION=1_0_14
IF NOT EXIST "%DEPS%\qt\jom\jom.exe". (
    cd "%DEPS%"
    IF NOT EXIST jom_%JOM_VERSION%.zip. (
        cecho {0D}Downloading JOM build tool for Qt.{# #}{\n}
        wget http://download.qt-project.org/official_releases/jom/jom_%JOM_VERSION%.zip
        IF NOT %ERRORLEVEL%==0 GOTO :ERROR
    )

    cecho {0D}Installing JOM build tool for to %DEPS%\qt\jom.{# #}{\n}
    mkdir %DEPS%\qt\jom
    7za x -y -oqt\jom jom_%JOM_VERSION%.zip
) ELSE (
    cecho {0D}JOM already installed to %DEPS%\qt\jom. Skipping.{# #}{\n}
)

:: Set QMAKESPEC and QTDIR in case we are going to build qt. If we don't do this
:: a system set QMAKESPEC might take over the build in some bizarre fashion.
:: Note 1: QTDIR is not used while build, neither should QMAKESPEC be used when -platform is given to configure.
:: Note 2: We cannot do this inside the qt IF without @setlocal EnableDelayedExpansion.
set QTDIR=%DEPS%\qt\qt-src-%QT_VER%
set QMAKESPEC=%QTDIR%\mkspecs\%QT_PLATFORM%
set QT_LIB_INFIX=-vc9-x86-
set QTNETWORK4_DLL="%QTDIR%\lib\QtNetwork%QT_LIB_INFIX%4.dll"

IF NOT EXIST %QTNETWORK4_DLL%. (
    IF NOT EXIST "%QTDIR%". (
        cecho {0E}Warning: %QTDIR% does not exist, extracting Qt failed?.{# #}{\n}
        GOTO :ERROR
    )

    cd %QTDIR%
    IF EXIST configure.cache. del /Q configure.cache
    cecho {0D}Configuring Qt build. Please answer 'y'!{# #}{\n}
    configure -platform %QT_PLATFORM% -debug-and-release -opensource -prefix "%DEPS%\qt" -shared -ltcg ^
        -no-qt3support -no-opengl -no-openvg -no-dbus -no-phonon -no-phonon-backend -no-multimedia -no-audio-backend ^
        -no-declarative -no-xmlpatterns -no-webkit -no-script -no-scripttools -nomake examples -nomake demos -nomake tools ^
        -qt-zlib -qt-libpng -qt-libmng -qt-libjpeg -qt-libtiff -qtlibinfix %QT_LIB_INFIX% -no-openssl
    IF NOT %ERRORLEVEL%==0 GOTO :ERROR

    cecho {0D}Building Debug and Release Qt with jom. Please be patient, this will take a while.{# #}{\n}
    "%DEPS%\qt\jom\jom.exe"
    IF NOT %ERRORLEVEL%==0 GOTO :ERROR

    IF NOT EXIST %QTNETWORK4_DLL%. (
        cecho {0E}Warning: %QTDIR%\lib\QtNetwork4.dll not present, Qt build failed?.{# #}{\n}
        GOTO :ERROR
    )

    :: Don't use jom for install. It seems to hang easily, maybe beacuse it tries to use multiple cores.
    cecho {0D}Installing Qt to %DEPS%\qt{# #}{\n}
    nmake install
    IF NOT %ERRORLEVEL%==0 GOTO :ERROR

    IF NOT EXIST %QTNETWORK4_DLL%. (
        cecho {0E}Warning: %QTNETWORK4_DLL% not present, Qt install failed?.{# #}{\n}
        GOTO :ERROR
    )
    IF NOT %ERRORLEVEL%==0 GOTO :ERROR
) ELSE (
    cecho {0D}Qt %QT_VER% already built. Skipping.{# #}{\n}
)

cecho {0D}Deploying QtCore and QtNetwork DLLs to Tundra bin\.{# #}{\n}
copy /Y "%DEPS%\qt\bin\QtCore%QT_LIB_INFIX%*.dll" %TUNDRA_BIN%
copy /Y "%DEPS%\qt\bin\QtNetwork%QT_LIB_INFIX%*.dll" %TUNDRA_BIN%
IF NOT %ERRORLEVEL%==0 GOTO :ERROR

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
