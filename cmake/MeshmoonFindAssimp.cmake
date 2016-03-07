
# Sane default for all platforms. Cant be 
# overridden with env variable ASSIMP_DIR.
set (ASSIMP_DIR ${ENV_TUNDRA_DEP_PATH}/assimp)

macro (use_package_assimp)
    if (WIN32 OR APPLE)
        if ("${ENV_ASSIMP_DIR}" STREQUAL "")
           set(ASSIMP_DIR ${ENV_TUNDRA_DEP_PATH}/assimp)
        else ()
            message (STATUS "-- Using from env variable ASSIMP_DIR")
            set (ASSIMP_DIR ${ENV_ASSIMP_DIR})
        endif()
        include_directories (${ASSIMP_DIR}/include)
        link_directories (${ASSIMP_DIR}/lib)
    else() # Linux
        if ("${ENV_ASSIMP_DIR}" STREQUAL "")
            set(ASSIMP_DIR ${ENV_TUNDRA_DEP_PATH})
        else()
            message (STATUS "-- Using from env variable ASSIMP_DIR")
            set (ASSIMP_DIR ${ENV_ASSIMP_DIR})
        endif()
        include_directories (${ASSIMP_DIR}/include/assimp)
        link_directories (${ASSIMP_DIR}/lib)
    endif()
endmacro()

macro (link_package_assimp)
    target_link_libraries (${TARGET_NAME} optimized assimp)
    if (WIN32)
        target_link_libraries (${TARGET_NAME} debug assimpd)
    endif()
endmacro ()
