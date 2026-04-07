include_guard(GLOBAL)

# ensures all variables are set either through CMake or environment
function(HouUtilsValidateConfig)
    include(core_utils)
    
    EnsureVariable("HOUDINI_INSTALL_PATH" FALSE TRUE)

    # normalize path before checking if it exists
    if (NOT EXISTS "${HOUDINI_INSTALL_PATH}")
        message(FATAL_ERROR "'HOUDINI_INSTALL_PATH' was set but the directory is invalid: ${HOUDINI_INSTALL_PATH}")
    endif()

    set(HOUDINI_LIB_PATH "${HOUDINI_INSTALL_PATH}/custom/houdini/dsolib" )

    if (NOT EXISTS "${HOUDINI_LIB_PATH}")
        message(FATAL_ERROR "'HOUDINI_LIB_PATH' is not a valid directory: ${HOUDINI_LIB_PATH}. Ensure that 'HOUDINI_INSTALL_PATH' is set correctly: ${HOUDINI_INSTALL_PATH}")
    endif()

    EnsureVariable("CUSTOM_DSO_PATH" TRUE TRUE)
    EnsureVariable("CUSTOM_USD_DSO_PATH" TRUE TRUE)

    # verify the full environment variables as well
    EnsureVariable("HOUDINI_DSO_PATH" TRUE TRUE)
    EnsureVariable("HOUDINI_USD_DSO_PATH" TRUE TRUE)

    # make paths into cmake cache variables
    set(HOUDINI_INSTALL_PATH "${HOUDINI_INSTALL_PATH}" CACHE PATH "Houdini Installation Path")
    set(HOUDINI_LIB_PATH "${HOUDINI_LIB_PATH}"  CACHE INTERNAL "Houdini DSO Lib Path")
    # mark these as `FORCE` so they are not actually cached, but do not use `INTERNAL` for easy visibility in GUIs
    set(CUSTOM_DSO_PATH "${CUSTOM_DSO_PATH}" CACHE PATH "Custom DSO Search Path (SET VIA ENV, NOT CMAKE)" FORCE)
    set(CUSTOM_USD_DSO_PATH "${CUSTOM_USD_DSO_PATH}" CACHE PATH "Custom USD DSO Search Path (SET VIA ENV, NOT CMAKE)" FORCE)

    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        set(HOUDINI_DEBUGGER_CMD "${HOUDINI_INSTALL_PATH}/bin/houdini.exe" 
            CACHE STRING "Command to use for debugging of targets built for Houdini integration.")
        set(HOUDINI_DEBUGGER_CMD_ARGS "${HOUDINI_DEBUGGER_CMD_ARGS}"
            CACHE STRING "Command-line Arguments to use for debugging of targets built for Houdini integration."
        )
    endif()
endfunction()

function(HouUtilsValidatePackage)
    if(NOT DEFINED _houdini_include_dir)
        message(FATAL_ERROR "`HoudiniConfig.cmake` was found, but internal variable name '_houdini_include_dir' seems to have changed.")
    endif()
    if(NOT DEFINED _python_include_dir)
        message(FATAL_ERROR "`HoudiniConfig.cmake` was found, but internal variable name '_python_include_dir' seems to have changed.")
    endif()

    # make includes directories list available
    set(Houdini_INCLUDES "${_houdini_include_dir}" "${_python_include_dir}" PARENT_SCOPE)
endfunction()

# print all useful variables to command-line
function(HouUtilsAnnounceState)
    include(core_utils)

    message(STATUS "Successful configuration of CMake environment for Houdini in '${PROJECT_NAME}'.")
    AnnounceVariable(HOUDINI_INSTALL_PATH)
    AnnounceVariable(HOUDINI_LIB_PATH)
    AnnounceVariable(CUSTOM_DSO_PATH)
    AnnounceVariable(CUSTOM_USD_DSO_PATH)

    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        AnnounceVariable(HOUDINI_DEBUGGER_CMD)
        AnnounceVariable(HOUDINI_DEBUGGER_CMD_ARGS)
    endif()

    message(STATUS "Located `HoudiniConfig.cmake` and retrieved following CMake variables:")
    AnnounceVariable(Houdini_FOUND)
    AnnounceVariable(Houdini_DIR)
    AnnounceVariable(Houdini_VERSION)
    AnnounceVariable(Houdini_VERSION_MAJOR)
    AnnounceVariable(Houdini_VERSION_MINOR)
    AnnounceVariable(Houdini_VERSION_PATCH)
    AnnounceVariable(Houdini_INCLUDES)
endfunction()