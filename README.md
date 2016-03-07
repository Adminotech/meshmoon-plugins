# Meshmoon client C++ plugins

Meshmoon Rocket is a the native Meshmoon client application. This repository contains plugins and Entity Components that provide Rocket its functionality on top of realXtend Tundra.

## Open sourcing notes

This open source version omits certain business logic parts from RocketPlugin. Otherwise code is provided as is and can be utilized in any Tundra distros. Meshmoon server side, the actual cloud hosting, utilizes this same base Rocket client with additional server business logic. These server plugins are not open sources at this time. They includes many services, servers and other infrastructure that is out of scope for open sourcing.

The RocketPlugin spesific codebase is a reference implementation of how you would build a Meshmoon like service. That being said, even if the "cloud parts" are not implemented, this client is fully capable of hosting a standards Tundra server with all the additional Meshmoon plugins and Entity Components. You may use the Rocket executable to run a server and connect clients to it. So it is a valid option to run your own server on the web and connect clients to it, with all the extended functionality of Meshmoon in this repo.

## Cloning

First you need the Meshmoon version of Tundra. It is the same codebase as realXtend Tundra but with additional features and bug fixes.

```
# Clone base Tundra
cd <projects>
git clone git@github.com:Adminotech/tundra.git meshmoon-tundra

# Clone Meshmoon plugins
cd meshmoon-tundra/src
git clone git@github.com:Adminotech/meshmoon-plugins.git
```

## Dependencies

Remember to first follow the Tundra build instructions in `<projects>/meshmoon-tundra`. Check out the scripts in `tools/<OS>`.

### Windows Visual Studio 2010 64bit

64bit Windows builds can only be produced with Visual Studio 2010. But you also need Visual Studio 2008 as CEF and subset of Qt needs to be built as 32bit VC9 binary. These are isolated into their own processes, so your Tundra can still be a 64bit build. If you don't have VC9 you can skip this step, these few components are disabled during runtime.

Run the following in a VS 2008 command prompt

```
cd <projects>/meshmoon-tundra/src/meshmoon-plugins

cd deps/Windows/VS2008
BuildDeps-x86.cmd
BuildQt-x86.cmd
```

Now run the main build in a VS 2010 command prompt

```
cd <projects>/meshmoon-tundra/src/meshmoon-plugins
cd deps/Windows/VS2010
BuildDeps-x64.cmd
```

### Windows Visual Studio 2008 32bit

Run the build script. The above 64bit complexities are automatically omitted and attached into the main Tundra build as your VC9 tools supports them.

```
cd <projects>/meshmoon-tundra/src/meshmoon-plugins

cd deps/Windows/VS2008
BuildDeps-x86.cmd
```

### Linux and Mac OS X

@todo

## SilverLining and Triton SDK

Meshmoon Sky and Water components use these commercial libraries. If you wish to distribute them you need to buy lisences, you can also find trial SDKs to try them out from http://sundog-soft.com

Set `TRITON_PATH` and `SILVERLINING_PATH` env variables to the root of the installed SDKs. If these are not set, they will be automatically excluded from the build, still providing the Entity Components but with no op implementations. They can be enabled separately, you don't have to have them both.

SilverLining SDK needs to v3.x, newer ones are untested and not guaranteed to work. Latest tested version is 3.011 (July 24, 2014).
Triton SDK needs to v2.x, newer ones are untested and not guaranteed to work. Latest tested version is (2.90 July 30, 2014).

## Building

The `meshmoon-plugins` directory is automatically picked up by Adminotech Tundra. As per Tundra instructions, open up your Visual Studio command prompt and run the following. For Mac OS X and Linux follow the Tundra instructions.

```
cd <projects>/meshmoon-tundra
cmake-{vs-version}{-arch}.bat
```

Open up the solution with Visual Studio and build.
