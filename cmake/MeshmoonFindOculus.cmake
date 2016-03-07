
set (OCULUS_FOUND FALSE)
set (OCULUS_DK2_FOUND FALSE)

macro (configure_oculus)
    if (ROCKET_OCULUS_ENABLED)
        if (NOT "$ENV{OCULUS_SDK}" STREQUAL "")
            file (TO_CMAKE_PATH "$ENV{OCULUS_SDK}" OCULUS_SDK)
        else ()
            message (FATAL_ERROR "Environment variable OCULUS_SDK is empty, it should point to the root of Oculus SDK 0.2.5c. Define it or set ROCKET_OCULUS_ENABLED to false to disable Oculus from the build.")
        endif ()

        if (MSVC)
            set (OCULUS_FOUND TRUE)

            # Detect DK1 vs. DK2 SDK version
            set (OCULUS_LIBRARY_DIR ${OCULUS_SDK}/LibOVR/Lib/${VS_PLATFORM})
            if (IS_DIRECTORY "${OCULUS_LIBRARY_DIR}/VS2010")
                set (OCULUS_LIBRARY_DIR ${OCULUS_LIBRARY_DIR}/VS2010)
                set (OCULUS_DK2_FOUND TRUE)
            endif ()

            # Library
            set (OCULUS_LIBRARY_NAME ${OCULUS_LIBRARY_DIR}/libovr)
            if (CMAKE_CL_64)
                set (OCULUS_LIBRARY_NAME ${OCULUS_LIBRARY_NAME}64)
            endif ()

            # Verify its there
            if (NOT EXISTS "${OCULUS_LIBRARY_NAME}.lib")
                message (FATAL_ERROR "Oculus library could not be found from ${OCULUS_LIBRARY_NAME}.lib")
            endif()

            set (OCULUS_LIBRARIES Winmm optimized ${OCULUS_LIBRARY_NAME}.lib debug ${OCULUS_LIBRARY_NAME}d.lib)

            # Verify VC10
            if (OCULUS_DK2_FOUND AND MSVC90)
                message(WARNING "Oculus SDK 0.4.x no longer supports Visual Studio 2008. Oculus functinality will be disabled.")
                set (OCULUS_FOUND FALSE)
                set (ROCKET_OCULUS_ENABLED FALSE)
            endif ()
        elseif (APPLE)
            # TODO: DK1 vs. DK2 SDK version
            set (OCULUS_LIBRARIES optimized ${OCULUS_SDK}/LibOVR/Lib/MacOS/Release/libovr.a debug ${OCULUS_SDK}/LibOVR/Lib/MacOS/Debug/libovr.a)
        else ()
            message (FATAL_ERROR "Oculus VR SDK not supported on this platform for Rocket")
        endif ()
    endif ()
endmacro ()

macro (use_oculus)
    if (OCULUS_FOUND)
        include_directories (${OCULUS_SDK}/LibOVR/Include ${OCULUS_SDK}/LibOVR/Src)
    endif ()
endmacro ()

macro (link_oculus)
    if (OCULUS_FOUND)
        target_link_libraries (${TARGET_NAME} ${OCULUS_LIBRARIES})
    endif ()
endmacro ()

macro (declare_oculus_defines)
    if (ROCKET_OCULUS_ENABLED AND OCULUS_FOUND)
        add_definitions (-DROCKET_OCULUS_ENABLED)
        if (OCULUS_DK2_FOUND)
            add_definitions (-DROCKET_OCULUS_DK2_ENABLED)
        endif()
    endif()
endmacro ()
