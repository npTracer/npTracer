include_guard(GLOBAL)

function(EnsureVariable variable_name env_only is_path)
    if(env_only)
        if (DEFINED ENV{${variable_name}})
            set(${variable_name} "$ENV{${variable_name}}")
        else()
            message(FATAL_ERROR "'${variable_name}' was not set. Set as an environment variable.")
        endif()
    else()
        if (DEFINED ENV{${variable_name}})
            set(${variable_name} "$ENV{${variable_name}}")
        endif()
        if(NOT DEFINED ${variable_name})
            message(FATAL_ERROR "'${variable_name}' was not set. Set either as an environment variable or a CMake variable.")
        endif()
        set(${variable_name} "${${variable_name}}")
    endif()
    if(is_path)
        cmake_path(CONVERT "${${variable_name}}" TO_CMAKE_PATH_LIST ${variable_name} NORMALIZE)
    endif()
        
    set(${variable_name} "${${variable_name}}" PARENT_NAME)
endfunction()

function(ConfigureFilePostBuild target_name)
    set(options)

    set(oneValueArgs
        IN_PATH
        OUT_PATH
        GENERATOR_MODULE_PATH
    )

    set(multiValueArgs
        CONFIGURE_VARIABLES
    )

    cmake_parse_arguments(arg
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DCONFIGURE_VARIABLES="${arg_CONFIGURE_VARIABLES}"
            -DIN_PATH="${arg_IN_PATH}"
            -DOUT_PATH="${arg_OUT_PATH}"
            -P "${arg_GENERATOR_MODULE_PATH}"
    )
endfunction()

function(ExportHeaders target_name base_src_dir base_dest_dir) # input headers list as `ARGN`
    foreach(header ${ARGN})
        # compute relative path to base directory
        file(RELATIVE_PATH REL_PATH "${base_src_dir}" "${header}")

        # destination path is the relative path joined to destination directory
        set(DEST_PATH "${base_dest_dir}/${REL_PATH}")

        # file-specific destination directory
        get_filename_component(DEST_DIR "${DEST_PATH}" DIRECTORY)

        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DEST_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${header}"
                "${DEST_PATH}"
            COMMENT "Copied '${header}' to '${DEST_PATH}'."
        )
    endforeach()
endfunction()

# copy headers to an explicit directory layout within the binary directory so that includes can use a prefix path
function(AddPrefixToTargetIncludes target_name)
    set(EXPORTED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
    set(EXPORTED_HEADER_DIR "${EXPORTED_INCLUDE_DIR}/${target_name}")

    ExportHeaders(${target_name} "${CMAKE_CURRENT_SOURCE_DIR}" "${EXPORTED_HEADER_DIR}" ${ARGN})

    target_include_directories(${target_name} PUBLIC "${EXPORTED_INCLUDE_DIR}")
endfunction()

function(FilterInstallableTargets out_variable_name)
    set(${out_variable_name} "")
    message(STATUS "Listing all current targets created by '${CMAKE_PROJECT_NAME}'...")

    foreach(tgt ${ARGN})
        get_target_property(TGT_TYPE ${tgt} TYPE)

        message(STATUS "${tgt}: ${TGT_TYPE}")

        if(TGT_TYPE STREQUAL "EXECUTABLE" OR
            TGT_TYPE STREQUAL "STATIC_LIBRARY" OR
            TGT_TYPE STREQUAL "SHARED_LIBRARY" OR
            TGT_TYPE STREQUAL "MODULE_LIBRARY" OR
            TGT_TYPE STREQUAL "INTERFACE_LIBRARY")
                
            list(APPEND INSTALLABLE_EXTERNAL_TARGETS ${tgt})
        endif()
    endforeach()

    set(${out_variable_name} ${${out_variable_name}} PARENT_SCOPE)
endfunction()

function(SetupInstallationAfterBuild target_name install_dir install_component)
    # ensure install command retains configurations
    if(IS_MULTI_CONFIG)
        set(CMAKE_CMD_ARGS --config=$<CONFIG>)
    else()
        set(CMAKE_CMD_ARGS -DCMAKE_BUILD_TYPE=$<CONFIG>)
    endif()

    set(CMAKE_CONSTANT_CMD_ARGS --install "${CMAKE_BINARY_DIR}" --prefix "${install_dir}")
    
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} ${CMAKE_CONSTANT_CMD_ARGS} ${CMAKE_CMD_ARGS} --component ${CMAKE_INSTALL_DEFAULT_COMPONENT_NAME} && ${CMAKE_COMMAND} ${CMAKE_CONSTANT_CMD_ARGS} ${CMAKE_CMD_ARGS} --component ${install_component}
        COMMENT "Installed '${target_name}' after building to '${install_dir}'."
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
endfunction()

function (InvertBooleanVariable var_value out_var_name)
    if(${var_value})
        set(${out_var_name} FALSE PARENT_SCOPE)
    else()
        set(${out_var_name} TRUE PARENT_SCOPE)
    endif()
endfunction()