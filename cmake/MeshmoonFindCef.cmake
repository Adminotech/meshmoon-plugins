
set (CEF_REQUIRED TRUE)
set (CEF_FOUND FALSE)
set (CEF3_FOUND FALSE)
set (CEF3_SANDBOX FALSE)
set (CEF_RUNTIME_FILES_INSTALL "")

macro (find_cef)
    if (NOT CEF_FOUND)
        if ("${CEF_ROOT}" STREQUAL "")
            if ("$ENV{CEF_ROOT}" STREQUAL "")
                # todo Mac
                if (IS_DIRECTORY "${AdminoPlugins_Deps_DIR}/cef")
                    set (CEF_ROOT ${AdminoPlugins_Deps_DIR}/cef)
                endif()
            else()
                file (TO_CMAKE_PATH "$ENV{CEF_ROOT}" CEF_ROOT)
            endif ()
            set (CEF_ROOT ${CEF_ROOT} CACHE PATH "CEF_ROOT dependency path" FORCE)
        endif ()
        if (NOT "${CEF_ROOT}" STREQUAL "" AND IS_DIRECTORY "${CEF_ROOT}")
            set (CEF_FOUND TRUE)
            if (IS_DIRECTORY "${CEF_ROOT}/out")
                set (CEF3_FOUND TRUE)
                # todo Currently this wont link as its build against static runtime.
                #if (MSVC10)
                #    set (CEF3_SANDBOX TRUE)
                #endif()
            endif ()
            copy_cef_runtime ()
        elseif (CEF_REQUIRED)
            message(FATAL_ERROR "CEF is a mandatory dependency and the resolved CEF_ROOT '${CEF_ROOT}' does not exist.")
        endif ()
    endif ()
endmacro ()

macro (copy_cef_runtime)
    if ("${CEF_RUNTIME_FILES}" STREQUAL "")
        if (NOT CEF3_FOUND)
            # todo Mac
            if (MSVC)
                # todo Do we need to copy d3dx9_43.dll and d3dcompiler_43.dll on windows???
                set (CEF_RUNTIME_FILES
                    ${CEF_ROOT}/Release/libcef.dll
                    ${CEF_ROOT}/Release/icudt.dll
                    ${CEF_ROOT}/Release/libEGL.dll
                    ${CEF_ROOT}/Release/libGLESv2.dll
                )
                set (CEF_RUNTIME_FILES_INSTALL "libcef.dll;icudt.dll;libEGL.dll;libGLESv2.dll")
                file (GLOB CEF_LOCALE_FILES 
                    "${CEF_ROOT}/Release/locales/*"
                )
                set (CEF_PAK_FILES 
                    ${CEF_ROOT}/Release/devtools_resources.pak
                )
                file (MAKE_DIRECTORY 
                    ${TUNDRA_BIN}/data/cef
                    ${TUNDRA_BIN}/data/cef/locales
                )
                file (COPY ${CEF_RUNTIME_FILES} DESTINATION ${TUNDRA_BIN}/)
                file (COPY ${CEF_PAK_FILES}     DESTINATION ${TUNDRA_BIN}/data/cef/)
                file (COPY ${CEF_LOCALE_FILES}  DESTINATION ${TUNDRA_BIN}/data/cef/locales/)
            endif ()
        else ()
            # todo Mac
            if (MSVC)
                # todo Do we need to copy d3dcompiler_43.dll and d3dcompiler_46.dll on windows???
                set (CEF_RUNTIME_FILES
                    ${CEF_ROOT}/Release/libcef.dll
                    ${CEF_ROOT}/Release/libEGL.dll
                    ${CEF_ROOT}/Release/libGLESv2.dll
                    ${CEF_ROOT}/Release/ffmpegsumo.dll
                    ${CEF_ROOT}/Release/pdf.dll
                    ${CEF_ROOT}/Resources/icudtl.dat
                )
                set (CEF_RUNTIME_FILES_INSTALL "libcef.dll;libEGL.dll;libGLESv2.dll;ffmpegsumo.dll;pdf.dll;icudtl.dat")
                file (GLOB CEF_LOCALE_FILES 
                    "${CEF_ROOT}/Resources/locales/*"
                )
                file (GLOB CEF_PAK_FILES 
                    "${CEF_ROOT}/Resources/*.pak"
                )
                file (MAKE_DIRECTORY 
                    ${TUNDRA_BIN}/data/cef
                    ${TUNDRA_BIN}/data/cef/locales
                )
                file (COPY ${CEF_RUNTIME_FILES} DESTINATION ${TUNDRA_BIN}/)
                file (COPY ${CEF_PAK_FILES}     DESTINATION ${TUNDRA_BIN}/data/cef/)
                file (COPY ${CEF_LOCALE_FILES}  DESTINATION ${TUNDRA_BIN}/data/cef/locales/)
            endif ()
        endif ()
    endif ()
endmacro ()

macro (use_package_cef)
    find_cef ()

    include_directories (${CEF_ROOT} ${CEF_ROOT}/include ${CEF_ROOT}/libcef_dll)        
endmacro ()

macro (link_package_cef)
    find_cef ()

    if (WIN32)
        if (NOT CEF3_FOUND)
            target_link_libraries (${TARGET_NAME} optimized ${CEF_ROOT}/lib/Release/libcef.lib debug ${CEF_ROOT}/lib/Debug/libcef.lib)
            target_link_libraries (${TARGET_NAME} optimized ${CEF_ROOT}/Release/lib/libcef_dll_wrapper.lib debug ${CEF_ROOT}/Debug/lib/libcef_dll_wrapper.lib)
        else()
            target_link_libraries (${TARGET_NAME} optimized ${CEF_ROOT}/Release/libcef.lib debug ${CEF_ROOT}/Debug/libcef.lib)
            if (CEF3_SANDBOX)
                target_link_libraries (${TARGET_NAME} optimized ${CEF_ROOT}/Release/cef_sandbox.lib debug ${CEF_ROOT}/Debug/cef_sandbox.lib)
            endif ()
            target_link_libraries (${TARGET_NAME} optimized ${CEF_ROOT}/out/Release/lib/libcef_dll_wrapper.lib debug ${CEF_ROOT}/out/Debug/lib/libcef_dll_wrapper.lib)
        endif()
    else()
        target_link_libraries (${TARGET_NAME} optimized ${CEF_ROOT}/Release/libcef debug ${CEF_ROOT}/Debug/libcef)
        # todo: target_link_libraries (${TARGET_NAME} optimized ${CEF_ROOT}/Release/lib/libcef_dll_wrapper.lib debug ${CEF_ROOT}/Debug/lib/libcef_dll_wrapper.lib)
    endif ()
endmacro ()

macro (declare_cef_defines)
    if (CEF_FOUND AND CEF3_FOUND)
        add_definitions(-DROCKET_CEF3_ENABLED)
        if (CEF3_SANDBOX)
            add_definitions(-DROCKET_CEF3_SANDBOX_ENABLED)
        endif ()
    endif ()
endmacro ()
