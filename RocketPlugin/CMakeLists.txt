
configure_oculus ()
find_crunch_executable ()

set  (DEPS ${ENV_TUNDRA_DEP_PATH})

# Define target name and output directory
init_target (RocketPlugin OUTPUT plugins)

# Define source files
file (GLOB CPP_FILES *.cpp)
file (GLOB H_FILES *.h)
file (GLOB UI_FILES ui/*.ui)
file (GLOB RESOURCE_FILES ui/*.qrc)

if (CMAKE_BUILD_TYPE MATCHES RelWithDebInfo OR CMAKE_BUILD_TYPE MATCHES Debug)
    set(ENABLE_SPARKLE_UPDATER FALSE)
else()
    set(ENABLE_SPARKLE_UPDATER TRUE)
endif()

# /updater
file(GLOB H_FILES_UPDATER updater/RocketUpdater.h)
file(GLOB CPP_FILES_UPDATER updater/RocketUpdater.cpp)
if (APPLE)
    if (ENABLE_SPARKLE_UPDATER)
        add_definitions(-DMAC_ENABLE_UPDATER)
        set(H_FILES_UPDATER ${H_FILES_UPDATER} updater/AutoUpdater.h updater/CocoaInitializer.h
            updater/SparkleAutoUpdater.h updater/UpdaterDelegate.h)
        set(CPP_FILES_UPDATER ${CPP_FILES_UPDATER} updater/AutoUpdater.cpp updater/CocoaInitializer.mm
            updater/SparkleAutoUpdater.mm updater/UpdaterDelegate.mm)
    endif()
endif ()
source_group("Header Files\\updater" FILES ${H_FILES_UPDATER})
source_group("Source Files\\updater" FILES ${CPP_FILES_UPDATER})

AddSourceFolder(presis)
AddSourceFolder(cave)
AddSourceFolder(buildmode)
AddSourceFolder(storage)
AddSourceFolder(editors)
AddSourceFolder(occlusion)
AddSourceFolder(oculus)
AddSourceFolder(utils)
AddSourceFolder(rendering)

# /media copy to bin
file (COPY media/meshmoon DESTINATION ${TUNDRA_BIN}/media)
# /scenes copy to bin
file (COPY scenes DESTINATION ${TUNDRA_BIN})

set (SOURCE_FILES ${CPP_FILES} ${H_FILES} ${CPP_FILES_UPDATER} ${H_FILES_UPDATER})

# Pass almost every header for MOC
file(GLOB MOC_FILES Rocket*.h Meshmoon*.h updater/Rocket*.h presis/Rocket*.h cave/Rocket*.h
     buildmode/Rocket*.h storage/*.h editors/*.h occlusion/Rocket*.h oculus/Rocket*.h
     utils/Rocket*.h)

# With a few exceptions
list(REMOVE_ITEM MOC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/MeshmoonData.h)
list(REMOVE_ITEM MOC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/RocketFwd.h)
list(REMOVE_ITEM MOC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/utils/RocketAnimationsFwd.h)
list(REMOVE_ITEM MOC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/RocketScriptTypeDefines.h)
list(REMOVE_ITEM MOC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/MeshmoonScriptTypeDefines.h)
list(REMOVE_ITEM MOC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/presis/RocketSplineCurve3D.h)

QT4_WRAP_CPP(MOC_SRCS ${MOC_FILES})
QT4_WRAP_UI (UI_SRCS ${UI_FILES})
QT4_ADD_RESOURCES(RESOURCE_SRCS ${RESOURCE_FILES})

MocFolder ()
UiFolder ()
source_group("UI Files" FILES ${UI_FILES})

# Includes
UseTundraCore ()
use_core_modules(TundraCore Math OgreRenderingModule TundraProtocolModule ECEditorModule
                 EnvironmentModule PhysicsModule SceneInteract)
use_app_modules (JavascriptModule AvatarModule SkyXHydrax SceneInteract DebugStatsModule)

use_triton ()
use_silverlining ()
use_oculus ()
use_qts3 ()

include_directories (${MeshmoonCommon_DIR})
include_directories (${MeshmoonComponents_DIR})
include_directories (${MeshmoonHttpPlugin_DIR})
include_directories (${MeshmoonEnvironmentPlugin_DIR})

if (ROCKET_OPENNI AND OPENNI_FOUND)
    include_directories (${RocketOpenNIPlugin_DIR})
endif ()

build_library (${TARGET_NAME} SHARED ${SOURCE_FILES} ${MOC_SRCS} ${UI_SRCS} ${RESOURCE_SRCS})

# Linking
link_ogre ()
link_package_knet ()
link_oculus ()
link_qts3 ()
link_modules (TundraCore Math OgreRenderingModule TundraProtocolModule ECEditorModule JavascriptModule
              AvatarModule EnvironmentModule SkyXHydrax PhysicsModule DebugStatsModule
              MeshmoonCommon MeshmoonComponents MeshmoonHttpPlugin MeshmoonEnvironmentPlugin)

if (ROCKET_OPENNI AND OPENNI_FOUND)
    link_modules(RocketOpenNIPlugin)
endif()

declare_openni_defines ()
declare_oculus_defines ()

# Copy Rocket startup config
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/rocket.json DESTINATION ${TUNDRA_BIN}/)

if (WIN32)
    file (COPY
        ${CRUNCH_EXECUTABLE}
        ${PROJECT_BINARY_DIR}/tools/Windows/Utils/7za.exe
        DESTINATION ${TUNDRA_BIN}/
    )
elseif (APPLE)
    # Sparkle for auto updater mac implementation
    if (ENABLE_SPARKLE_UPDATER)
        configure_sparkle ()
        link_package (SPARKLE)
    endif()
endif()

SetupCompileFlags()

final_target ()

install (CODE "message(\" \")")
install (CODE "message(STATUS \"Installing RocketPlugin\")")
install (CODE "message(STATUS \"-----------------------\")")

# Needed executables
if (MSVC)
    setup_install_files (${PROJECT_BINARY_DIR}/tools/Windows/Utils/7za.exe)
    setup_install_files (${CRUNCH_EXECUTABLE})
endif ()

# Licenses. Should be in <RocketPlugin>/licenses subfolder and
# copied in the install step to <tundra_bin>/licenses
function (copy_rocket_licence sourceDir sourceFilename destFilename)
    if (EXISTS ${sourceDir}/${sourceFilename})
        file (COPY ${sourceDir}/${sourceFilename} DESTINATION ${ROCKET_LICENSES})
        file (RENAME ${ROCKET_LICENSES}/${sourceFilename} ${ROCKET_LICENSES}/${destFilename})
    elseif (NOT ARGV3)
        message(FATAL_ERROR "Cannot install license ${sourceDir}/${sourceFilename}, file does not exist!")
    endif()
endfunction ()

set  (ROCKET_LICENSES ${CMAKE_CURRENT_SOURCE_DIR}/licenses)
set  (PKG_LICENSES_OPTIONAL FALSE)
file (MAKE_DIRECTORY ${ROCKET_LICENSES})

if (LINUX)
    set (DEPS ${DEPS}/src)
    set (PKG_LICENSES_OPTIONAL TRUE)
endif()

file (TO_CMAKE_PATH "$ENV{TRITON_PATH}" TRITON_SDK)
file (TO_CMAKE_PATH "$ENV{SILVERLINING_PATH}" SILVERLINING_SDK)
if (NOT "${TRITON_SDK}" STREQUAL "")
    copy_rocket_licence (${TRITON_SDK} license.txt Triton.txt)
endif ()
if (NOT "${SILVERLINING_SDK}" STREQUAL "")
    copy_rocket_licence (${SILVERLINING_SDK} license.txt SilverLining.txt)
endif ()

# Licenses
setup_remove_file(MumbleLicence.txt)
copy_rocket_licence (${PROJECT_BINARY_DIR}/src/Application/MumblePlugin/mumble MumbleLicence.txt Mumble.txt)
copy_rocket_licence (${PROJECT_BINARY_DIR}/src/Core/OgreRenderingModule/libcrunch license.txt Crunch.txt)
copy_rocket_licence (${DEPS}/assimp LICENSE Assimp.txt)
copy_rocket_licence (${DEPS}/bullet COPYING Bullet.txt)
copy_rocket_licence (${DEPS}/celt COPYING Celt.txt)
copy_rocket_licence (${DEPS}/ogre-safe-nocrashes COPYING Ogre.txt)
copy_rocket_licence (${DEPS}/qtscriptgenerator LICENSE.LGPL QtScriptGenerator.txt)
copy_rocket_licence (${DEPS}/qjson COPYING.lib QJson.txt)
copy_rocket_licence (${DEPS}/protobuf COPYING.txt Protobuf.txt PKG_LICENSES_OPTIONAL)
copy_rocket_licence (${DEPS}/ogg COPYING Ogg.txt PKG_LICENSES_OPTIONAL)
copy_rocket_licence (${DEPS}/speex COPYING Speex.txt PKG_LICENSES_OPTIONAL)
copy_rocket_licence (${DEPS}/theora COPYING Theora.txt PKG_LICENSES_OPTIONAL)
copy_rocket_licence (${DEPS}/vorbis COPYING Vorbis.txt PKG_LICENSES_OPTIONAL)

# Boost as a special case with TUNDRA_NO_BOOST
if (NOT TUNDRA_NO_BOOST AND EXISTS ${DEPS}/boost/LICENSE_1_0.txt)
    copy_rocket_licence(${DEPS}/boost LICENSE_1_0.txt Boost.txt)
endif()
if (TUNDRA_NO_BOOST)
    file (REMOVE ${ROCKET_LICENSES}/Boost.txt)
    file (REMOVE ${TUNDRA_BIN}/licenses/Boost.txt)
endif ()

# Deps path specific to MSVC
if (MSVC)
    if (EXISTS ${DEPS}/qt/qt-src-4.8.5)
        copy_rocket_licence (${DEPS}/qt/qt-src-4.8.5 LICENSE.LGPL Qt.txt) # todo: Remove once everyone has built the new qt...
    elseif (EXISTS ${DEPS}/qt/qt-src-4.8.6)
        copy_rocket_licence (${DEPS}/qt/qt-src-4.8.6 LICENSE.LGPL Qt.txt)
    else ()
        message(FATAL_ERROR "Could not resolve Qt source location from ${DEPS}/qt for license.")
    endif ()

    if (EXISTS ${DEPS}/ogre-safe-nocrashes/RenderSystems/Headless)
        copy_rocket_licence (${DEPS}/ogre-safe-nocrashes/RenderSystems/Headless LICENSE OgreHeadlessRenderer.txt)
    endif ()
    copy_rocket_licence (${DEPS}/zziplib/zziplib-0.13.59/docs COPYING.ZLIB Zziplib.txt)
    copy_rocket_licence (${CEF_ROOT} LICENSE.txt Cef.txt)
    copy_rocket_licence (${DEPS}/realxtend-tundra-deps/hydrax License.txt Hydrax.txt)
    copy_rocket_licence (${DEPS}/realxtend-tundra-deps/skyx License.txt SkyX.txt)
    copy_rocket_licence (${DEPS}/openssl/src LICENSE OpenSSL.txt)
# Deps path specific for Mac
elseif (APPLE)
    copy_rocket_licence (${DEPS}/zziplib COPYING.ZLIB Zziplib.txt)
    copy_rocket_licence (${DEPS}/hydrax License.txt Hydrax.txt)
    copy_rocket_licence (${DEPS}/skyx License.txt SkyX.txt)
    copy_rocket_licence (${DEPS}/../build/cef_binary_1.1180.1069_macosx LICENSE.txt Cef.txt)
endif()

# Copy licenses to /bin and execute on install step.
file (GLOB ROCKET_LICENSES_FILES ${ROCKET_LICENSES}/*)
file (COPY ${ROCKET_LICENSES_FILES} DESTINATION ${TUNDRA_BIN}/licenses)
setup_install_files_find(${ROCKET_LICENSES} *.* "" licenses)

# Startup, eula and DLL deps.
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/doc/EULA-Meshmoon-Rocket.rtf DESTINATION ${TUNDRA_BIN})
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/doc/EULA-Meshmoon-Rocket.txt DESTINATION ${TUNDRA_BIN})

setup_install_files (${CMAKE_CURRENT_SOURCE_DIR}/rocket.json)
setup_install_files (${CMAKE_CURRENT_SOURCE_DIR}/doc/EULA-Meshmoon-Rocket.rtf)
setup_install_files (${CMAKE_CURRENT_SOURCE_DIR}/doc/EULA-Meshmoon-Rocket.txt)
setup_install_windows_deps ("qts3.dll" Release|RelWithDebInfo "qts3d.dll" Debug)
