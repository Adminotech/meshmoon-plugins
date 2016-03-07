#!/bin/bash
set -e

# Utilities
printUsage()
{
    echo " "
    echo "USAGE: $0 [--client <PATH> --help]"
    echo "   or: $0 [      -c <PATH> -h]"
    echo " "
    echo " -h | --help                   Prints this message"
    echo " "
    echo " -c <PATH> | --client <PATH>   Specifies the path to Tundra working directory."
    echo "                               Note: You can hardcode Tundra working directory in CLIENT variable"
    echo "                                     in this script to avoid setting this argument all the time."
    echo " "
    echo " --- The above options, if specified, will always override CLIENT variable ---"
    echo " "
}

printUsageAndDie()
{
    printUsage
    exit 1
}

echoInfo()
{
    COLOR="\033[32;1m"
    ENDCOLOR="\033[0m"
    echo -e $COLOR INFO: $@ $ENDCOLOR
}

echoWarn()
{
    COLOR="\033[33;1m"
    ENDCOLOR="\033[0m"
    echo -e $COLOR WARNING: $@ $ENDCOLOR
}

echoError()
{
    COLOR="\033[31;1m"
    ENDCOLOR="\033[0m"
    echo -e $COLOR ERROR: $@ $ENDCOLOR
}

die()
{
    echoError "An error occured! Aborting!"
    exit 1
}

NO_ARGS="0"
ERRORS_OCCURED="0"

ORIGINAL_DIR=`pwd`
CLIENT=$HOME/workspace/tundra # Specify tundra path here

# Deps-specific variables
CEF_VERSION="cef_binary_1.1180.1069_macosx"

echo " "
echo "=========== Adminotech Components dependency building script ==========="
echo "=                                                                      ="
echo "= This script will build the following dependencies:                   ="
echo "= - Chromium Embedded Framework (CEF)                                  ="
echo "=                                                                      ="
echo "========================================================================"
echo " "

if [ "$#" -ne "$NO_ARGS" ]; then
    echoInfo "Chosen options: $@"
    echo " "
    while [ "$1" != "" ]; do
        case $1 in
            -h | --help )           printUsageAndDie
                                    ;;
            -c | --client )         shift
                                    if [ ! -d "$1" ]; then
                                        echoError "Invalid or non-existent path for client: $1"
                                        ERRORS_OCCURED="1"
                                        shift
                                        continue
                                    fi
                                    CLIENT=$1
                                    ;;
            * )                     echoError "Invalid option: $1"
                                    ERRORS_OCCURED="1"
                                    shift
                                    continue
        esac
        shift
    done
fi

if [ "$ERRORS_OCCURED" == "1" ]; then printUsageAndDie; fi

DEPS=$CLIENT/deps
PREFIX=$DEPS/osx-64

if [ ! -d "$DEPS" ]; then
    echoError "Invalid or non-existent path for DEPS: $DEPS"
    ERRORS_OCCURED="1"
fi

if [ ! -d "$CLIENT" ]; then
    echoError "Invalid or non-existant path for CLIENT: $CLIENT"
    ERRORS_OCCURED="1"
fi

if [ "$ERRORS_OCCURED" == "1" ]; then die; fi

echo " "
echoInfo "Target DEPS path: $DEPS"
echoInfo "Target CLIENT path: $CLIENT"
echo " "

BUILD=$DEPS/build
TARBALLS=$DEPS/tarballs
mkdir -p $BUILD $TARBALLS $DEPS

if [ ! -d "$BUILD/$CEF_VERSION" ]; then
    cd $BUILD
    ZIP=$CEF_VERSION.zip
    ZIP_PATH=$TARBALLS/$ZIP

    if [ ! -f $ZIP_PATH ]; then
        echoInfo "Fetching CEF, this could take a while..."
        curl -L -o $ZIP_PATH http://meshmoon.data.s3.amazonaws.com/deps/$ZIP || die
    fi

    tar xzf $ZIP_PATH || die
fi

cd $BUILD/$CEF_VERSION

if [ ! -f "xcodebuild/Release/libcef_dll_wrapper.a" ]; then
    xcodebuild -configuration Release -target libcef_dll_wrapper SDKROOT=macosx10.7 ARCHS=i386 VALID_ARCHS=i386 ONLY_ACTIVE_ARCH=YES INSTALL_PATH=$PREFIX/libcef_dll_wrapper/Release GCC_TREAT_WARNINGS_AS_ERRORS=NO GCC_VERSION=com.apple.compilers.llvm.clang.1_0
fi

if [ ! -f "xcodebuild/Debug/libcef_dll_wrapper.a" ]; then
    xcodebuild -configuration Debug -target libcef_dll_wrapper SDKROOT=macosx10.7 ARCHS=i386 VALID_ARCHS=i386 ONLY_ACTIVE_ARCH=YES INSTALL_PATH=$PREFIX/libcef_dll_wrapper/Debug GCC_TREAT_WARNINGS_AS_ERRORS=NO GCC_VERSION=com.apple.compilers.llvm.clang.1_0
fi

test -f $CLIENT/bin/libcef.dylib || cp $BUILD/$CEF_VERSION/Release/libcef.dylib $CLIENT/bin/

BROWSER_PROJECT="$ORIGINAL_DIR/../rocketwebbrowser/BrowserTemplate.pro"

if [ -f $BROWSER_PROJECT ]; then
    sed -e 's|CEF_DIR_HERE|'$BUILD/$CEF_VERSION'|g' < $BROWSER_PROJECT > Browser.pro
fi


echoInfo "CEF building is done."
