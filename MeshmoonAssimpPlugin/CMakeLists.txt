
# Define target name and output directory
init_target (MeshmoonAssimpPlugin OUTPUT plugins)

# Define source files
file (GLOB CPP_FILES *.cpp)
file (GLOB H_FILES   *.h)
file (GLOB MOC_FILES MeshmoonAssimpPlugin.h 
                     MeshmoonOpenAssetImporter.h)

QT4_WRAP_CPP (MOC_SRCS ${MOC_FILES})

MocFolder()

set (SOURCE_FILES ${CPP_FILES} ${H_FILES} ${MOC_SRCS})

# Includes
use_package_assimp ()
UseTundraCore ()
use_core_modules (TundraCore Math OgreRenderingModule)

build_library (${TARGET_NAME} SHARED ${SOURCE_FILES})

# Linking

link_ogre ()
link_package_assimp ()
link_package_knet ()
link_modules (TundraCore Math OgreRenderingModule)

SetupCompileFlagsWithPCH()

final_target ()

install (CODE "message(\" \")")
install (CODE "message(STATUS \"Installing ${TARGET_NAME}\")")
install (CODE "message(STATUS \"-------------------------------\")")

setup_install_windows_deps ("assimp.dll" Release|RelWithDebInfo "assimpD.dll" Debug)
