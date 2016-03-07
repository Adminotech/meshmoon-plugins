#!/bin/bash

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 <PATH_TO_QT_32BIT_BUILD>"
    exit 1
fi

# set this path to point to Qt 32 bit build
QT32DIR=$1

if [ ! -d "$QT32DIR" ] && [ ! -f "$QT32DIR/bin/qmake" ]; then
    echo "Please set QT32DIR path to a valid Qt 32-bit build prior to running this script!"
    exit 1
fi

export QTDIR=$QT32DIR
export PATH=$PATH:$QT32DIR/bin

WEBBROWSER_PATH=$PWD/../rocketwebbrowser
OLDDIR=$PWD
cd $WEBBROWSER_PATH
WEBBROWSER_PATH=$PWD

if [ ! -f "$WEBBROWSER_PATH/Browser.pro" ]; then
    echo "Browser.pro not found! Build the dependencies first by running build-deps-mac.bash script!"
    exit 1
fi

qmake $WEBBROWSER_PATH/Browser.pro -r -spec unsupported/macx-clang CONFIG+=release CONFIG+=x86
make -j4

echo "RocketWebBrowser built successfully."

cd $OLDDIR
