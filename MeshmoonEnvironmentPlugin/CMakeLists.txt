
# Detect if Triton SDK was found and if full build should be executed

#message ("MESHMOON_TRITON ${MESHMOON_TRITON}")
#message ("MESHMOON_SERVER_BUILD ${MESHMOON_SERVER_BUILD}")
#message ("TRITON_FOUND ${TRITON_FOUND}")

set (MESHMOON_TRITON TRUE)
set (MESHMOON_SILVERLINING TRUE)

if (MESHMOON_SERVER_BUILD)
    set (MESHMOON_TRITON FALSE)
    set (MESHMOON_SILVERLINING FALSE)
endif ()

# Define target name and output directory
init_target (MeshmoonEnvironmentPlugin OUTPUT plugins)

# Define source files

file(GLOB CPP_FILES *.cpp)
file(GLOB H_FILES *.h)
file(GLOB MOC_FILES EC_*.h)

set (SOURCE_FILES ${CPP_FILES} ${H_FILES} ${CPP_FILES_UPDATER} ${H_FILES_UPDATER})

QT4_WRAP_CPP (MOC_SRCS ${MOC_FILES})

MocFolder ()

# Includes
UseTundraCore ()
use_core_modules (TundraCore OgreRenderingModule Math)
use_app_modules (SkyXHydrax)

if (MESHMOON_TRITON)
    use_triton ()
    if (NOT TRITON_FOUND)
        set (MESHMOON_TRITON FALSE)
    else ()
        add_definitions (-DMESHMOON_TRITON)
    endif ()
endif ()

if (MESHMOON_SILVERLINING)
    use_silverlining ()
    if (NOT SILVERLINING_FOUND)
        set (MESHMOON_SILVERLINING FALSE)
    else ()
        add_definitions (-DMESHMOON_SILVERLINING)
    endif ()
endif ()

add_definitions (-DMeshmoonEnvironmentPlugin_EXPORTS)

build_library (${TARGET_NAME} SHARED ${SOURCE_FILES} ${MOC_SRCS})

# Linking
link_ogre ()
link_modules (TundraCore OgreRenderingModule SkyXHydrax Math)

if (MESHMOON_TRITON OR MESHMOON_SILVERLINING)

    if (MESHMOON_TRITON)
        link_triton ()
    endif ()

    if (MESHMOON_SILVERLINING)
        link_silverlining ()
    endif ()

    # Windows
    if (MSVC)
        if (MESHMOON_TRITON)
            # Copy Triton resources to bin
            string (TOLOWER ${VC_VER} TRITON_VC_VER_LOWER)
            string (TOLOWER ${VS_PLATFORM} TRITON_VS_PLATFORM_LOWER)
            set  (TRITON_PLATFORM_PATH ${TRITON_VC_VER_LOWER}/${TRITON_VS_PLATFORM_LOWER})
            file (MAKE_DIRECTORY ${TUNDRA_BIN}/media/meshmoon/triton)
            file (MAKE_DIRECTORY ${TUNDRA_BIN}/media/meshmoon/triton/${TRITON_PLATFORM_PATH})
            file (GLOB TRITON_RESOURCES ${TRITON_SDK}/Resources/*.*)
            file (GLOB TRITON_RUNTIME ${TRITON_SDK}/Resources/${TRITON_PLATFORM_PATH}/*-DLL.dll)
            file (COPY ${TRITON_RESOURCES} DESTINATION ${TUNDRA_BIN}/media/meshmoon/triton)
            file (COPY ${TRITON_RUNTIME} DESTINATION ${TUNDRA_BIN}/media/meshmoon/triton/${TRITON_PLATFORM_PATH})

            # Copy runtime dependencies, what gets found might be used depending on the hardware
            set (TRITON_RUNTIME_DIR dll)
            if (CMAKE_CL_64)
                set (TRITON_RUNTIME_DIR dll64)
            endif ()
            file (COPY ${TRITON_SDK}/Resources/${TRITON_RUNTIME_DIR} DESTINATION ${TUNDRA_BIN}/media/meshmoon/triton)
        endif ()

        if (MESHMOON_SILVERLINING)
            # Copy SilverLining resources to bin
            string (TOUPPER ${VC_VER} SL_VC_VER_UPPER)
            string (TOLOWER ${VS_PLATFORM} SL_VS_PLATFORM_LOWER)
            set  (SL_PLATFORM_PATH ${SL_VC_VER_UPPER}/${SL_VS_PLATFORM_LOWER})
            file (MAKE_DIRECTORY ${TUNDRA_BIN}/media/meshmoon/silverlining)
            file (MAKE_DIRECTORY ${TUNDRA_BIN}/media/meshmoon/silverlining/${SL_PLATFORM_PATH})
            file (GLOB SL_RESOURCES ${SILVERLINING_SDK}/resources/*.*)
            file (GLOB SL_RUNTIME ${SILVERLINING_SDK}/resources/${SL_PLATFORM_PATH}/SilverLiningDirectX9-*-DLL.dll 
                                  ${SILVERLINING_SDK}/resources/${SL_PLATFORM_PATH}/SilverLiningOpenGL-*-DLL.dll)
            file (COPY ${SL_RESOURCES} DESTINATION ${TUNDRA_BIN}/media/meshmoon/silverlining)
            file (COPY ${SL_RUNTIME} DESTINATION ${TUNDRA_BIN}/media/meshmoon/silverlining/${SL_PLATFORM_PATH})

            # Copy resource subdirs
            set (SL_RESOURCE_SUBDIRS Clouds lensflare rain Shaders sleet snow)
            foreach (SL_RESOURCE_SUBDIR ${SL_RESOURCE_SUBDIRS})
                file (COPY ${SILVERLINING_SDK}/resources/${SL_RESOURCE_SUBDIR} DESTINATION ${TUNDRA_BIN}/media/meshmoon/silverlining)
            endforeach ()
        endif ()
    # Mac
    elseif (APPLE)
        if (MESHMOON_TRITON)
            file (MAKE_DIRECTORY ${TUNDRA_BIN}/media/meshmoon/triton)
            file (GLOB TRITON_RESOURCES ${TRITON_SDK}/Resources/*)
            file (COPY ${TRITON_RESOURCES} DESTINATION ${TUNDRA_BIN}/media/meshmoon/triton)
        endif ()

        if (MESHMOON_SILVERLINING)
            file (MAKE_DIRECTORY ${TUNDRA_BIN}/media/meshmoon/silverlining)
            file (GLOB SL_RESOURCES ${SILVERLINING_SDK}/resources/*)
            file (COPY ${SL_RESOURCES} DESTINATION ${TUNDRA_BIN}/media/meshmoon/silverlining)
        endif ()

        include_directories (${ENV_TUNDRA_DEP_PATH}/../build/ogre-safe-nocrashes/RenderSystems/GL/include)
        target_link_libraries (${TARGET_NAME} ${TARGET_NAME} ${TUNDRA_BIN}/Contents/Frameworks/RenderSystem_GL.framework)
    # Linux
    elseif (UNIX)
        if (MESHMOON_TRITON)
            file (MAKE_DIRECTORY ${TUNDRA_BIN}/media/meshmoon/triton)
            file (GLOB TRITON_RESOURCES ${TRITON_SDK}/Resources/*)
            file (COPY ${TRITON_RESOURCES} DESTINATION ${TUNDRA_BIN}/media/meshmoon/triton)
        endif ()

        if (MESHMOON_SILVERLINING)
            file (MAKE_DIRECTORY ${TUNDRA_BIN}/media/meshmoon/silverlining)
            file (GLOB SL_RESOURCES ${SILVERLINING_SDK}/Resources/*)
            file (COPY ${SL_RESOURCES} DESTINATION ${TUNDRA_BIN}/media/meshmoon/silverlining)
        endif ()
    endif ()
endif ()

SetupCompileFlagsWithPCH ()
final_target ()
