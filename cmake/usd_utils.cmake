function(ConfigureUSDPluginTarget target_name install_dir)
    set(options)

    set(oneValueArgs
        DEBUG_CMD
        DEBUG_CMD_ARGS
    )

    set(multiValueArgs
        LIBS
        INCLUDES
    )

    cmake_parse_arguments(arg
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )

    set(PLUG_INFO_JSON_OUT_PATH "${install_dir}/plugInfo.json")
    set(CURR_LIBRARIES ${ARGN}) # all remaining arguments

    add_library(${target_name} SHARED ${USD_PLUG_SOURCES} ${USD_PLUG_HEADERS})

    target_link_libraries(${target_name}
        ${arg_LIBS}
        ${RENDERER_TARGET_NAME}
    )

    target_include_directories(${target_name}
        PRIVATE
            "${NPTracerPlugin_ROOT_DIR}"
        PUBLIC
            ${arg_INCLUDES}
    )

    # configure file directly to installation dir
    ConfigureFilePostBuild(${target_name}
        IN_PATH
            "${PLUG_INFO_JSON_IN_PATH}"
        OUT_PATH
            "${PLUG_INFO_JSON_OUT_PATH}"
        GENERATOR_MODULE_PATH
            "${NPTracer_CMAKE_MODULE_PATH}/GenerateFilePostBuild.cmake"
        CONFIGURE_VARIABLES
            PLUG_INFO_LIBRARY_PATH=$<TARGET_FILE_NAME:${target_name}>
            PLUG_INFO_LIBRARY_NAME=$<TARGET_FILE_BASE_NAME:${target_name}>
    )

    # set a compile definition for whether debugging is enabled
    target_compile_definitions(${target_name} PUBLIC "NPTRACER_DEBUG=$<BOOL:${NPTracerPlugin_USD_PLUG_DEBUG}>")
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
    )
    SetupInstallationAfterBuild(${target_name} ${install_dir}) # install as a post-build command

    # set some useful VS settings for QOL
    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        set_target_properties(${target_name} PROPERTIES VS_DEBUGGER_COMMAND "${arg_DEBUG_CMD}")
        
        set_target_properties(${target_name} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "${arg_DEBUG_CMD_ARGS}")
    endif()
endfunction()