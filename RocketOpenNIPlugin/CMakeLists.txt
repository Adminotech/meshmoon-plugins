#########################################################

# Define target name and output directory
init_target (RocketOpenNIPlugin OUTPUT plugins)

# Define source files
file (GLOB CPP_FILES *.cpp)
file (GLOB H_FILES *.h)
file (GLOB UI_FILES ui/*.ui)
file (GLOB RESOURCE_FILES ui/*.qrc)

set (SOURCE_FILES ${CPP_FILES} ${H_FILES} ${CPP_FILES_UPDATER} ${H_FILES_UPDATER})

# Specify headers for MOC.
file(GLOB MOC_FILES Rocket*.h TrackingUser.h TrackingSkeleton.h OpenNIDevice.h)
QT4_WRAP_CPP(MOC_SRCS ${MOC_FILES})
QT4_WRAP_UI (UI_SRCS ${UI_FILES})
QT4_ADD_RESOURCES(RESOURCE_SRCS ${RESOURCE_FILES})

MocFolder ()
UiFolder ()
source_group("UI Files" FILES ${UI_FILES})

# Includes
use_package_openni ()
UseTundraCore ()
use_core_modules(TundraCore Math)

add_definitions (-DOPENNI_MODULE_EXPORTS)

build_library (${TARGET_NAME} SHARED ${SOURCE_FILES} ${MOC_SRCS} ${UI_SRCS} ${RESOURCE_SRCS})

# Linking
link_package_openni ()
link_ogre ()
link_modules (TundraCore Math)

# Copy OpenNI config
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/OpenNIConfig.xml DESTINATION ${TUNDRA_BIN}/)

SetupCompileFlagsWithPCH()

final_target ()
