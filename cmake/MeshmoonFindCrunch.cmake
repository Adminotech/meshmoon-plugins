
macro (find_crunch_executable)
    if (MSVC)
        set (CRUNCH_EXECUTABLE "${RocketPlugin_DIR}/deps/crunch/win/crunch.exe")
        if (EXISTS "${AdminoPlugins_Deps_DIR}/crunch/bin/crunch.exe")
            set (CRUNCH_EXECUTABLE "${AdminoPlugins_Deps_DIR}/crunch/bin/crunch.exe")
        endif ()
    endif ()
endmacro ()
