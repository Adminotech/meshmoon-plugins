
# Define target name and output directory
init_target (MeshmoonCommon OUTPUT plugins)

# Define source files
file (GLOB CPP_FILES *.cpp)
file (GLOB H_FILES   *.h)
file (GLOB MOC_FILES MeshmoonCommonPlugin.h
                     common/MeshmoonCommon.h
                     common/loaders/*.h
                     common/layers/*.h)

AddSourceFolder(common)
AddSourceFolder(common/loaders)
AddSourceFolder(common/layers)
AddSourceFolder(common/script)
AddSourceFolder(common/json)

QT4_WRAP_CPP (MOC_SRCS ${MOC_FILES})

MocFolder()

set (SOURCE_FILES ${CPP_FILES} ${H_FILES} ${MOC_SRCS})

# Includes
UseTundraCore ()
use_core_modules (TundraCore Math TundraProtocolModule OgreRenderingModule)
use_entity_components (EC_HoveringText EC_Sound)

include_directories(${MeshmoonHttpPlugin_DIR})

build_library (${TARGET_NAME} SHARED ${SOURCE_FILES})

# Linking
link_package_knet ()
link_modules (TundraCore Math TundraProtocolModule OgreRenderingModule MeshmoonHttpPlugin)
link_entity_components (EC_HoveringText EC_Sound)

SetupCompileFlagsWithPCH()

final_target ()
