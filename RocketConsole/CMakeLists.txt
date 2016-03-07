# Define target name and output directory
init_target(RocketConsole OUTPUT ./)

# Define source files
file (GLOB CPP_FILES main.cpp)
file (GLOB H_FILES "") # This project has no headers.
set (SOURCE_FILES ${CPP_FILES} ${H_FILES})
set (RESOURCE_FILES "")

set (FILES_TO_TRANSLATE ${FILES_TO_TRANSLATE} ${H_FILES} ${CPP_FILES} PARENT_SCOPE)

if (MSVC)
    # Windows resource files for icon and version information.
    set(RESOURCE_FILES ${RESOURCE_FILES} ../Rocket/resource.h ../Rocket/Rocket.rc)
    # Preprocessor defines on MSVC
    set_property(DIRECTORY PROPERTY COMPILE_DEFINITIONS_RELEASE _DMEMDUMP)
    set_property(DIRECTORY PROPERTY COMPILE_DEFINITIONS_RELWITHDEBINFO _DMEMDUMP)
endif()

SetupCompileFlags()

UseTundraCore()
use_core_modules(TundraCore Math)

build_executable(${TARGET_NAME} ${SOURCE_FILES} ${RESOURCE_FILES})

if (MSVC)
    if (ROCKET_CRASH_REPORTER_ENABLED)
        include_directories (${CRASHRPT_INCLUDE_DIRS})
        target_link_libraries (${TARGET_NAME} ${CRASHRPT_LIBRARIES})
        target_link_libraries (${TARGET_NAME} ${QT_QTCORE_LIBRARY})
        add_definitions (-DROCKET_CRASH_REPORTER_ENABLED)
    endif ()

    if (CMAKE_CL_64)
        link_package_knet() # For kNet's BottomMemoryAllocator
    endif ()
endif()

link_modules(TundraCore)

final_target ()
