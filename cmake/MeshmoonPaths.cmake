
macro (resolve_meshmoon_plugins_base)
    set (MESHMOON_PATH_BASE "${CMAKE_CURRENT_SOURCE_DIR}/RocketPlugin")

    if (IS_DIRECTORY "${MESHMOON_PATH_BASE}")
        # Inside admino-plugins
        set (MESHMOON_PATH_BASE "${CMAKE_CURRENT_SOURCE_DIR}")
    else ()
        # Another repo in <repo>/src
        set (MESHMOON_PATH_BASE "${CMAKE_CURRENT_SOURCE_DIR}/../admino-plugins")
    endif ()
    if (NOT IS_DIRECTORY "${MESHMOON_PATH_BASE}")
        message(FATAL_ERROR "Failed to resolve admino-plugins repo root! Called from: ${CMAKE_CURRENT_SOURCE_DIR}")
    endif ()
endmacro ()

macro (configure_meshmoon_paths)
    message("Configuring Meshmoon Paths")

    resolve_meshmoon_plugins_base()
    
    set (MESHMOON_PLUGIN_NAMES
        MeshmoonCommon MeshmoonComponents
        MeshmoonAssimpPlugin MeshmoonHttpPlugin MeshmoonEnvironmentPlugin
        RocketPlugin RocketOpenNIPlugin
    )
    set (AdminoPlugins_DIR "${MESHMOON_PATH_BASE}")
    set (AdminoPlugins_Deps_DIR "")

    # Windows deps
    # todo: Would this be useful to set on mac as well?
    if (MSVC)
        set (AdminoPlugins_Deps_DIR "${MESHMOON_PATH_BASE}/deps")
        # Check for new style Rocket deps
        set (_new_deps_dir "${MESHMOON_PATH_BASE}/deps")
        if (MSVC10)
            set (_new_deps_dir "${_new_deps_dir}-vs2010")
        else()
            set (_new_deps_dir "${_new_deps_dir}-vs2008")
        endif()
        if (CMAKE_CL_64)
            set (_new_deps_dir "${_new_deps_dir}-x64")
        else()
            set (_new_deps_dir "${_new_deps_dir}-x86")
        endif()
        # Only use if some of the deps are built
        if (IS_DIRECTORY "${_new_deps_dir}" AND IS_DIRECTORY "${_new_deps_dir}/qts3" AND IS_DIRECTORY "${_new_deps_dir}/cef")
            set (AdminoPlugins_Deps_DIR "${_new_deps_dir}")
        endif ()
    endif()

    message(STATUS "Adminotech Plugins")
    message(STATUS "    ${AdminoPlugins_DIR}")
    message(STATUS "Adminotech Dependencies")
    message(STATUS "    ${AdminoPlugins_Deps_DIR}")
    message(" ")

    foreach(MESHMOON_PLUGIN_NAME ${MESHMOON_PLUGIN_NAMES})
        set (DIR_VAR_NAME ${MESHMOON_PLUGIN_NAME}_DIR)
        set (LOCAL_DIR_PATH "${MESHMOON_PATH_BASE}/${MESHMOON_PLUGIN_NAME}")
        set (${DIR_VAR_NAME} "${LOCAL_DIR_PATH}")
        if ("${LOCAL_DIR_PATH}" STREQUAL "" OR NOT IS_DIRECTORY "${LOCAL_DIR_PATH}")
            message(FATAL_ERROR "Failed to find '${MESHMOON_PLUGIN_NAME}', looked from '${LOCAL_DIR_PATH}'")
        else ()
            message(STATUS "${MESHMOON_PLUGIN_NAME}")
            message(STATUS "    ${${DIR_VAR_NAME}}")
        endif ()
    endforeach ()
    message(" ")
endmacro ()
