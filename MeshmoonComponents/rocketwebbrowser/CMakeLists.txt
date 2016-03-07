
# Define target name and output directory
init_target (RocketWebBrowser OUTPUT ./)

# Define source files
file (GLOB CPP_FILES_IPC *.cpp)
file (GLOB H_FILES_IPC *.h)
file (GLOB RES_FILES "")

QT4_WRAP_CPP(MOC_SRCS_IPC BrowserNetwork.h CefIntegration.h RocketWebBrowser.h)

if (WIN32)
    set(RES_FILES resources/resource.h resources/RocketWebBrowser.rc)
endif ()

set (SOURCE_FILES_IPC ${CPP_FILES_IPC} ${H_FILES_IPC} ${MOC_SRCS_IPC} ${RES_FILES})

MocFolder ()

# Includes
use_package_cef ()
use_package (QT4)

declare_cef_defines ()

build_executable (${TARGET_NAME} ${SOURCE_FILES_IPC})

# Linking
link_package_cef ()
link_package (QT4)

final_target ()

setup_install_windows_deps ("${CEF_RUNTIME_FILES_INSTALL}" Release|RelWithDebInfo|Debug)
