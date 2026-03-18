# ensures all variables are set either through CMake or environment
function(HouUtilsEnsureVariables)
    if (DEFINED ENV{HOUDINI_INSTALL_PATH})
        set(HOUDINI_INSTALL_PATH "$ENV{HOUDINI_INSTALL_PATH}")
    endif()
    if (NOT DEFINED HOUDINI_INSTALL_PATH)
        message(FATAL_ERROR "'HOUDINI_INSTALL_PATH' was not set. Set as either an environment variable or CMake variable.")
    endif()
    # normalize path before checking if it exists
    cmake_path(CONVERT "${HOUDINI_INSTALL_PATH}" TO_CMAKE_PATH_LIST HOUDINI_INSTALL_PATH NORMALIZE)
    if (NOT EXISTS "${HOUDINI_INSTALL_PATH}")
        message(FATAL_ERROR "'HOUDINI_INSTALL_PATH' was set but the directory is invalid: ${HOUDINI_INSTALL_PATH}")
    endif()

    set(HOUDINI_LIB_PATH "${HOUDINI_INSTALL_PATH}/custom/houdini/dsolib" )

    if (NOT EXISTS "${HOUDINI_LIB_PATH}")
        message(FATAL_ERROR "'HOUDINI_LIB_PATH' is not a valid directory: ${HOUDINI_LIB_PATH}. Ensure that 'HOUDINI_INSTALL_PATH' is set correctly: ${HOUDINI_INSTALL_PATH}")
    endif()

    if (DEFINED ENV{CUSTOM_DSO_PATH})
        set(CUSTOM_DSO_PATH "$ENV{CUSTOM_DSO_PATH}")
    endif()
    if (NOT DEFINED CUSTOM_DSO_PATH)
        message(FATAL_ERROR "'CUSTOM_DSO_PATH' was not set. Set as either an environment variable or CMake variable.")
    endif()
    # normalize path before checking if it exists
    cmake_path(CONVERT "${CUSTOM_DSO_PATH}" TO_CMAKE_PATH_LIST CUSTOM_DSO_PATH NORMALIZE)
    if (NOT EXISTS "${CUSTOM_DSO_PATH}")
        message(FATAL_ERROR "'CUSTOM_DSO_PATH' was set but the directory is invalid: ${CUSTOM_DSO_PATH}")
    endif()

     # make install path and dso path into cmake cache variables
    set(HOUDINI_INSTALL_PATH "${HOUDINI_INSTALL_PATH}" CACHE PATH "Houdini Installation Path")
    set(CUSTOM_DSO_PATH "${CUSTOM_DSO_PATH}" CACHE PATH "Custom DSO Search Path" FORCE) # mark as `FORCE` so it is not actually cached

    # make lib path accessible outside of function
    set(HOUDINI_LIB_PATH "${HOUDINI_LIB_PATH}" PARENT_SCOPE)
endfunction()

# print all useful variables to command-line
function(HouUtilsAnnounceState)
    message(STATUS "Successful configuration of Houdini CMake environment.")
    message(STATUS "HOUDINI_INSTALL_PATH: ${HOUDINI_INSTALL_PATH}")
    message(STATUS "HOUDINI_LIB_PATH: ${HOUDINI_LIB_PATH}")
    message(STATUS "CUSTOM_DSO_PATH: ${CUSTOM_DSO_PATH}")
    message(STATUS "Houdini_FOUND: ${Houdini_FOUND}")
    message(STATUS "Houdini_VERSION: ${Houdini_VERSION}")
    message(STATUS "Houdini_VERSION_MAJOR: ${Houdini_VERSION_MAJOR}")
    message(STATUS "Houdini_VERSION_MINOR: ${Houdini_VERSION_MINOR}")
    message(STATUS "Houdini_VERSION_PATCH: ${Houdini_VERSION_PATCH}")
endfunction()