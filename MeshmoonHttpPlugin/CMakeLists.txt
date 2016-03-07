
# Define target name and output directory
init_target (MeshmoonHttpPlugin OUTPUT plugins)

# Define source files
file (GLOB CPP_FILES *.cpp)
file (GLOB H_FILES *.h)
file (GLOB MOC_FILES MeshmoonHttpPlugin.h MeshmoonHttpClient.h MeshmoonHttpRequest.h)

set (SOURCE_FILES ${CPP_FILES} ${H_FILES})

QT4_WRAP_CPP(MOC_SRCS ${MOC_FILES})

MocFolder ()

# Includes
use_package(QT4)
UseTundraCore ()
use_app_modules (JavascriptModule)

build_library (${TARGET_NAME} SHARED ${SOURCE_FILES} ${MOC_SRCS})

# Linking
if (APPLE)
    link_ogre ()
endif ()
link_package(QT4)
link_modules (TundraCore JavascriptModule)

SetupCompileFlagsWithPCH()
final_target ()
