include_guard(GLOBAL)

# ensures all variables are set either through CMake or environment
function(HouUtilsEnsureVariables)
    include(core_utils)
    
    EnsureVariable("HOUDINI_INSTALL_PATH" FALSE)

    # normalize path before checking if it exists
    cmake_path(CONVERT "${HOUDINI_INSTALL_PATH}" TO_CMAKE_PATH_LIST HOUDINI_INSTALL_PATH NORMALIZE)
    if (NOT EXISTS "${HOUDINI_INSTALL_PATH}")
        message(FATAL_ERROR "'HOUDINI_INSTALL_PATH' was set but the directory is invalid: ${HOUDINI_INSTALL_PATH}")
    endif()

    set(HOUDINI_LIB_PATH "${HOUDINI_INSTALL_PATH}/custom/houdini/dsolib" )

    if (NOT EXISTS "${HOUDINI_LIB_PATH}")
        message(FATAL_ERROR "'HOUDINI_LIB_PATH' is not a valid directory: ${HOUDINI_LIB_PATH}. Ensure that 'HOUDINI_INSTALL_PATH' is set correctly: ${HOUDINI_INSTALL_PATH}")
    endif()

    EnsureVariable("CUSTOM_DSO_PATH" TRUE)
    # normalize path
    cmake_path(CONVERT "${CUSTOM_DSO_PATH}" TO_CMAKE_PATH_LIST CUSTOM_DSO_PATH NORMALIZE)

    # normalize path
    cmake_path(CONVERT "${CUSTOM_USD_DSO_PATH}" TO_CMAKE_PATH_LIST CUSTOM_USD_DSO_PATH NORMALIZE)

    EnsureVariable("CUSTOM_USD_DSO_PATH" TRUE)

    # verify the full environment variables as well
    EnsureVariable("HOUDINI_DSO_PATH" TRUE)
    EnsureVariable("HOUDINI_USD_DSO_PATH" TRUE)

    # make paths into cmake cache variables
    set(HOUDINI_INSTALL_PATH "${HOUDINI_INSTALL_PATH}" CACHE PATH "Houdini Installation Path")
    set(CUSTOM_DSO_PATH "${CUSTOM_DSO_PATH}" CACHE PATH "Custom DSO Search Path (SET VIA ENV, NOT CMAKE)" FORCE) # mark as `FORCE` so it is not actually cached
    set(CUSTOM_USD_DSO_PATH "${CUSTOM_USD_DSO_PATH}" CACHE PATH "Custom USD DSO Search Path (SET VIA ENV, NOT CMAKE)" FORCE)

    # make lib path accessible outside of function
    set(HOUDINI_LIB_PATH "${HOUDINI_LIB_PATH}" PARENT_SCOPE)
endfunction()

# print all useful variables to command-line
function(HouUtilsAnnounceState)
    message(STATUS "Successful configuration of Houdini CMake environment for '${PROJECT_NAME}'.")
    message(STATUS "HOUDINI_INSTALL_PATH: ${HOUDINI_INSTALL_PATH}")
    message(STATUS "HOUDINI_LIB_PATH: ${HOUDINI_LIB_PATH}")
    message(STATUS "CUSTOM_DSO_PATH: ${CUSTOM_DSO_PATH}")
    message(STATUS "CUSTOM_USD_DSO_PATH: ${CUSTOM_USD_DSO_PATH}")
    message(STATUS "Houdini_FOUND: ${Houdini_FOUND}")
    message(STATUS "Houdini_VERSION: ${Houdini_VERSION}")
    message(STATUS "Houdini_VERSION_MAJOR: ${Houdini_VERSION_MAJOR}")
    message(STATUS "Houdini_VERSION_MINOR: ${Houdini_VERSION_MINOR}")
    message(STATUS "Houdini_VERSION_PATCH: ${Houdini_VERSION_PATCH}")
endfunction()
