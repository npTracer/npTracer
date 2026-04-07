include_guard(GLOBAL)

function(ConfigureUSDPluginTarget target_name install_dir)
    set(options)

    set(oneValueArgs
        DEBUGGER_CMD
        DEBUGGER_CMD_ARGS
        DEBUGGER_CMD_ENV
        INSTALL_COMPONENT
        PLUG_INFO_IN_PATH
    )

    set(multiValueArgs
        USD_SOURCES
        USD_HEADERS
        LIBS
        INCLUDES
        DEFINES
    )

    cmake_parse_arguments(arg
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )

    set(PLUG_INFO_OUT_PATH "${install_dir}/plugInfo.json")
    set(CURR_LIBRARIES ${ARGN}) # all remaining arguments

    add_library(${target_name} SHARED ${arg_USD_SOURCES} ${arg_USD_HEADERS})

    target_link_libraries(${target_name} PRIVATE ${arg_LIBS})

    target_include_directories(${target_name} PRIVATE ${arg_INCLUDES})

    # configure file directly to installation dir
    ConfigureFilePostBuild(${target_name}
        IN_PATH
            "${arg_PLUG_INFO_IN_PATH}"
        OUT_PATH
            "${PLUG_INFO_OUT_PATH}"
        GENERATOR_MODULE_PATH
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GenerateFilePostBuild.cmake"
        CONFIGURE_VARIABLES
            PLUG_INFO_LIBRARY_PATH=$<TARGET_FILE_NAME:${target_name}>
            PLUG_INFO_LIBRARY_NAME=$<TARGET_FILE_BASE_NAME:${target_name}>
    )

    # set a compile definition for whether debugging is enabled
    target_compile_definitions(${target_name} PRIVATE 
        ${arg_DEFINES}
    )
    if(MSVC)
        target_compile_definitions(${target_name}
            PRIVATE
                "NOMINMAX"
                "WIN32_LEAN_AND_MEAN"
        )
    endif()

    # install
    install(TARGETS ${target_name}
        DESTINATION "."
        COMPONENT ${arg_INSTALL_COMPONENT}
    )

    SetupInstallationAfterBuild(${target_name} ${install_dir} ${arg_INSTALL_COMPONENT}) # install as a post-build command

    include(core_utils)

    ConfigureDebugger(${target_name} "${arg_DEBUGGER_CMD}" "${arg_DEBUGGER_CMD_ARGS}" "${arg_DEBUGGER_CMD_ENV}")
endfunction()

# print all useful variables to command-line
function(UsdUtilsValidate)
    include(core_utils)

    EnsureVariable("USD_INSTALL_PATH" FALSE TRUE)
    set(USD_INSTALL_PATH "${USD_INSTALL_PATH}" CACHE PATH "USD Installation Path")

    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        set(USD_STANDALONE_DEBUGGER_CMD "${USD_STANDALONE_DEBUGGER_CMD}"
            CACHE STRING "Command to use for debugging of standalone USD targets.")
        set(USD_STANDALONE_DEBUGGER_CMD_ARGS "${USD_STANDALONE_DEBUGGER_CMD_ARGS}" 
            CACHE STRING "Command-line Arguments to use for debugging of standalone USD targets.")
    endif()
endfunction()

function(UsdUtilsAnnounceState)
    message(STATUS "Successful configuration of CMake environment for USD in '${PROJECT_NAME}'.")
    AnnounceVariable(USD_INSTALL_PATH)

    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        AnnounceVariable(USD_STANDALONE_DEBUGGER_CMD)
        AnnounceVariable(USD_STANDALONE_DEBUGGER_CMD_ARGS)
    endif()
        
    message(STATUS "Located `pxrConfig.cmake` and retrieved following CMake variables:")
    AnnounceVariable(pxr_FOUND)
    AnnounceVariable(pxr_DIR)
    AnnounceVariable(PXR_VERSION)
    AnnounceVariable(PXR_MAJOR_VERSION)
    AnnounceVariable(PXR_MINOR_VERSION)
    AnnounceVariable(PXR_PATCH_VERSION)
    AnnounceVariable(PXR_INCLUDE_DIRS)
    AnnounceVariable(PXR_LIBRARIES)
endfunction()