
macro (use_qts3)
    if (NOT "$ENV{QTS3_HOME}" STREQUAL "")
        file (TO_CMAKE_PATH "$ENV{QTS3_HOME}" QTS3_HOME)
    elseif (EXISTS ${AdminoPlugins_Deps_DIR}/qts3)
        set (QTS3_HOME ${AdminoPlugins_Deps_DIR}/qts3)
    elseif (EXISTS ${ENV_TUNDRA_DEP_PATH}/src/qts3)
        set (QTS3_HOME ${ENV_TUNDRA_DEP_PATH}/src/qts3)
    else()
        message (FATAL_ERROR "Environment variable QTS3_HOME is empty and could not be resolved to a sane default. Run your admino-plugins deps script which should build the project to the correct place.")
    endif ()
    include_directories ("${QTS3_HOME}/include")
    if (MSVC)
        link_directories(${QTS3_HOME}/lib)
    endif()
endmacro ()

macro (link_qts3)
    if (MSVC)
        target_link_libraries (${TARGET_NAME} optimized qts3.lib debug qts3d.lib)
        file (COPY ${QTS3_HOME}/bin/Release/qts3.dll ${QTS3_HOME}/bin/Debug/qts3d.dll DESTINATION ${TUNDRA_BIN}/)
    elseif (APPLE)
        target_link_libraries (${TARGET_NAME} ${QTS3_HOME}/lib/libqts3.dylib)
        file (COPY ${QTS3_HOME}/lib/libqts3.dylib DESTINATION ${TUNDRA_BIN}/)
    elseif (UNIX)
        target_link_libraries (${TARGET_NAME} ${QTS3_HOME}/lib/libqts3.so)
        file (COPY ${QTS3_HOME}/lib/libqts3.so DESTINATION ${TUNDRA_BIN}/)
    endif()
endmacro ()
