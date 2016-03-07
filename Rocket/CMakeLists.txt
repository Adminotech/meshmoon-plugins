# Define target name and output directory
if (NOT ANDROID)
    init_target(Rocket OUTPUT ./)
else ()
    file(MAKE_DIRECTORY ${TUNDRA_BIN}/../android/libs)
    file(MAKE_DIRECTORY ${TUNDRA_BIN}/../android/libs/${ANDROID_ABI})
    init_target(Rocket OUTPUT ../android/libs/${ANDROID_ABI})
endif ()

# Define source files
file (GLOB CPP_FILES main.cpp)
file (GLOB H_FILES "") # Don't do *.h it will pickup Windows-spesific resource.h

set (SOURCE_FILES ${CPP_FILES} ${H_FILES})
set (RESOURCE_FILES "")

set (FILES_TO_TRANSLATE ${FILES_TO_TRANSLATE} ${H_FILES} ${CPP_FILES} PARENT_SCOPE)

if (MSVC)
    # Windows resource files for icon and version information.
    set(RESOURCE_FILES ${RESOURCE_FILES} resource.h Rocket.rc)
    # Preprocessor defines on MSVC
    set_property(DIRECTORY PROPERTY COMPILE_DEFINITIONS_RELEASE _DMEMDUMP)
    set_property(DIRECTORY PROPERTY COMPILE_DEFINITIONS_RELWITHDEBINFO _DMEMDUMP)
endif()

SetupCompileFlags()

UseTundraCore()
use_core_modules(TundraCore Math)

# On Android build the main program as a shared object
if (ANDROID)
   # Nasty CMake hack: the linker directories needed by the static modules have to be redeclared here. \todo Better solution
   use_package_bullet()
   build_library(${TARGET_NAME} SHARED ${SOURCE_FILES})
elseif (MSVC)
   build_executable(${TARGET_NAME} WIN32 ${SOURCE_FILES} ${RESOURCE_FILES})
else()
   build_executable(${TARGET_NAME} ${SOURCE_FILES} ${RESOURCE_FILES})
endif()

if (MSVC)
    if (ROCKET_CRASH_REPORTER_ENABLED)
        add_definitions (-DROCKET_CRASH_REPORTER_ENABLED)
        include_directories (${CRASHRPT_INCLUDE_DIRS})
        target_link_libraries (${TARGET_NAME} ${CRASHRPT_LIBRARIES})
        target_link_libraries (${TARGET_NAME} ${QT_QTCORE_LIBRARY})
        setup_install_windows_deps (
            "CrashRpt1402.dll;CrashSender1402.exe" Release|RelWithDebInfo
            "CrashRpt1402d.exe;CrashSender1402d.exe" Debug
        )
    endif ()

    if (CMAKE_CL_64)
        link_package_knet() # For kNet's BottomMemoryAllocator
    endif ()
endif()

link_modules(TundraCore)
# On Android all modules are linked as static.
if (ANDROID)
    link_modules(TundraProtocolModule OgreRenderingModule PhysicsModule EnvironmentModule AssetModule JavascriptModule AvatarModule DebugStatsModule)
endif()

final_target ()

install (CODE "message(\" \")")
install (CODE "message(STATUS \"Installing Meshmoon Rocket executable\")")
install (CODE "message(STATUS \"-------------------------------------\")")

# Install files
setup_install_files (${TUNDRA_BIN}/plugins/TundraInstallationDirectory.txt "plugins") # C++ plugins
setup_install_files (${TUNDRA_BIN}/../README.md)      # Readme
setup_install_files (${TUNDRA_BIN}/../LICENSE)        # Licence
setup_install_files (${TUNDRA_BIN}/../WhatsNew.txt)   # Whats new

# Remove stuff we don't want to ship with Rocket
install (CODE "message(\" \")")
install (CODE "message(STATUS \"Running Meshmoon Rocket post-install cleanup\")")
install (CODE "message(STATUS \"--------------------------------------------\")")

# Data
if (NOT UNIX)
    setup_remove_files_glob("data/assets/*" TRUE)
endif()
setup_remove_directory("data/ui/images/languages")
if (MSVC AND NOT ROCKET_CRASH_REPORTER_ENABLED)
    setup_remove_directory("data/crash-reporter")
endif ()
    
# Example scenes
setup_remove_directory("scenes")

# JS
setup_remove_directory("jsmodules/ConvertTundra1")
setup_remove_directory("jsmodules/ScriptImporter")

# Doc
setup_remove_directory("dox")

install (CODE "message(\" \")")
