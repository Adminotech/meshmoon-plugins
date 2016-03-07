
# VLC
macro(configure_vlc)
    if ("${VLC_ROOT}" STREQUAL "")
        file (TO_CMAKE_PATH "$ENV{VLC_ROOT}" VLC_ROOT)
    endif()
    if ("${VLC_ROOT}" STREQUAL "")
        SET(VLC_ROOT ${ENV_TUNDRA_DEP_PATH}/vlc)
    endif()
    
    sagase_configure_package(VLC
        NAMES vlc
        COMPONENTS vlc libvlc libvlccore
        PREFIXES ${VLC_ROOT})
    sagase_configure_report (VLC)
endmacro()

configure_vlc()

#########################################################

# Define target name and output directory
init_target (RocketMediaPlayer OUTPUT ./)

# Define source files
file (GLOB CPP_FILES_MEDIAPLAYER *.cpp)
file (GLOB H_FILES_MEDIAPLAYER *.h)
file (GLOB RES_FILES "")

QT4_WRAP_CPP (MOC_SRCS_MEDIAPLAYER MediaPlayerNetwork.h RocketMediaPlayer.h)

if (WIN32)
    set(RES_FILES resources/resource.h resources/RocketMediaPlayer.rc)
    if (MSVC90)
        include_directories (windows/)
    endif ()
endif ()

set (SOURCE_FILES_MEDIAPLAYER ${CPP_FILES_MEDIAPLAYER} ${H_FILES_MEDIAPLAYER} ${MOC_SRCS_MEDIAPLAYER} ${RES_FILES})

MocFolder ()

# Includes
use_package (VLC)
use_package (QT4)

build_executable (${TARGET_NAME} ${SOURCE_FILES_MEDIAPLAYER})

# Linking
link_package (VLC)
link_package (QT4)

# Disable certain optimizations for VLC as the libs get unstable when too much stuff is removed from them.
# I dont know if this is a bug in VLC or Microsoft tools that strip the unreferenced info away from the linked libs.
if (MSVC AND ENABLE_BUILD_OPTIMIZATIONS)
    set_target_properties (${TARGET_NAME} PROPERTIES LINK_FLAGS_RELEASE "/INCREMENTAL /OPT:NOREF")
    set_target_properties (${TARGET_NAME} PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/INCREMENTAL /OPT:NOREF")
endif ()

final_target ()

if (WIN32)
    setup_install_directory (${TUNDRA_BIN}/plugins/vlcplugins plugins)
    setup_install_windows_deps ("libvlc.dll;libvlccore.dll;axvlc.dll;npvlc.dll" Release|RelWithDebInfo|Debug)
    
    # TODO See what else can be removed without breaking VLC.
    setup_remove_directory("plugins/vlcplugins/gui")
    setup_remove_directory("plugins/vlcplugins/visualization")
    setup_remove_directory("plugins/vlcplugins/lua")
endif ()
