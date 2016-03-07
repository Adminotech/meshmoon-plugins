
macro (configure_sparkle)
    if (NOT APPLE)
        message(FATAL_ERROR "You are trying to configure Sparkle when not in Mac OS X?!")
    endif ()

    find_library (SPARKLE_LIBRARY NAMES Sparkle)
    set (SPARKLE_INCLUDE_DIRS ${SPARKLE_LIBRARY}/Headers)
    set (SPARKLE_LIBRARIES ${SPARKLE_LIBRARY})
endmacro (configure_sparkle)
