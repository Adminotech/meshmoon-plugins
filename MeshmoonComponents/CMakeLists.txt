
# At this point don't make finding CEF required.
# It will break the build of VC10 when using old dependencies.
# This hack can be removed once CEF3 integration is complete.
set (CEF_REQUIRED FALSE)
find_cef()
set (CEF_REQUIRED TRUE)

# Define target name and output directory
init_target (MeshmoonComponents OUTPUT plugins)

# Define source files
file (GLOB CPP_FILES *.cpp)
file (GLOB H_FILES *.h)
file (GLOB UI_FILES ui/*.ui rocketmediaplayer/ui/*.ui)
file (GLOB RESOURCE_FILES ui/*.qrc rocketmediaplayer/ui/*.qrc)

file(GLOB MOC_FILES MeshmoonComponents.h EC_*.h)

if (ROCKET_UMBRA_ENABLED)
    add_definitions(-DROCKET_UMBRA_ENABLED)
elif()
    list(REMOVE_ITEM MOC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/EC_MeshmoonOccluder.h)
    list(REMOVE_ITEM MOC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/EC_MeshmoonCulling.h)
endif ()

QT4_WRAP_CPP (MOC_SRCS ${MOC_FILES})
QT4_WRAP_UI (UI_SRCS ${UI_FILES})
QT4_ADD_RESOURCES (RESOURCE_SRCS ${RESOURCE_FILES})

UiFolder()
MocFolder()
source_group ("UI Files" FILES ${UI_FILES})

set (SOURCE_FILES ${CPP_FILES} ${H_FILES} ${MOC_SRCS} ${UI_SRCS} ${RESOURCE_SRCS})

# Includes
UseTundraCore ()
use_core_modules (TundraCore Math OgreRenderingModule TundraProtocolModule)
use_app_modules (SceneInteract)
use_entity_components (EC_Sound)
use_package_assimp ()
include_directories (${MeshmoonAssimpPlugin_DIR})

declare_cef_defines ()

build_library (${TARGET_NAME} SHARED ${SOURCE_FILES})

# Linking

link_ogre ()
link_package_knet ()
link_modules (TundraCore Math OgreRenderingModule TundraProtocolModule SceneInteract MeshmoonAssimpPlugin)
link_entity_components (EC_Sound)

if (WIN32)
    target_link_libraries (${TARGET_NAME} ws2_32.lib)
    if (NOT MESHMOON_SERVER_BUILD)
        target_link_libraries (${TARGET_NAME} optimized d3dx9.lib debug d3dx9d.lib)
    endif ()
elseif (APPLE)
    include_directories (${ENV_TUNDRA_DEP_PATH}/../build/ogre-safe-nocrashes/RenderSystems/GL/include)
    target_link_libraries (${TARGET_NAME} ${TUNDRA_BIN}/Contents/Frameworks/RenderSystem_GL.framework)
elseif (UNIX)
    if (NOT MESHMOON_SERVER_BUILD)
        target_link_libraries (${TARGET_NAME} ${OGRE_DIR}/lib/RenderSystem_GL.so GL)
    endif()
endif()

SetupCompileFlagsWithPCH()

final_target ()

install (CODE "message(\" \")")
install (CODE "message(STATUS \"Installing ${TARGET_NAME}\")")
install (CODE "message(STATUS \"-------------------------------\")")

# Windows: Build RocketWebBrowser and RocketMediaPlayer.
if (WIN32 AND NOT MESHMOON_SERVER_BUILD)
    # VC9: Run via cmake
    if (MSVC90)
        add_subdirectory (rocketwebbrowser)
        add_subdirectory (rocketmediaplayer)
    # >=VC10: Add install files for prebuilt executables and DLLs
    else ()
        # 32bit VC9 Qt
        setup_install_windows_deps (
            "QtCore-vc9-x86-4.dll;QtNetwork-vc9-x86-4.dll" Release|RelWithDebInfo
            "QtCore-vc9-x86-d4.dll;QtNetwork-vc9-x86-d4.dll" Debug
        )
        
        # RocketWebBrowser
        if (CMAKE_CL_64 AND CEF3_FOUND)
            add_subdirectory (rocketwebbrowser)
        else ()
            # Processes
            setup_install_windows_deps (
                "RocketWebBrowser.exe" Release|RelWithDebInfo
                "RocketWebBrowserd.exe" Debug
            )
            # 32bit VC9 CEF
            setup_install_windows_deps ("${CEF_RUNTIME_FILES_INSTALL}" Release|RelWithDebInfo|Debug)
        endif ()

        # RocketMediaPlayer
        setup_install_windows_deps (
            "RocketMediaPlayer.exe" Release|RelWithDebInfo
            "RocketMediaPlayerd.exe" Debug
        )
        # 32bit VC9 VLC
        setup_install_windows_deps (
            "libvlc.dll;libvlccore.dll;axvlc.dll;npvlc.dll" Release|RelWithDebInfo|Debug
        )

        setup_install_directory(${TUNDRA_BIN}/plugins/vlcplugins plugins)

        # TODO See what else can be removed without breaking VLC.
        setup_remove_directory("plugins/vlcplugins/gui")
        setup_remove_directory("plugins/vlcplugins/visualization")
        setup_remove_directory("plugins/vlcplugins/lua")
    endif ()
# Mac: Build RocketMediaPlayer. RocketWebBrowser is built in 32bit mode.
elseif (APPLE)
    add_subdirectory (rocketmediaplayer)
endif ()
