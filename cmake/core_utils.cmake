include_guard(GLOBAL)

function(EnsureVariable variable_name env_only)
    if(env_only)
        if (DEFINED ENV{${variable_name}})
            set(${variable_name} "$ENV{${variable_name}}" PARENT_SCOPE)
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
        set(${variable_name} "${${variable_name}}" PARENT_SCOPE)
    endif()
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

    cmake_parse_arguments(args
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DCONFIGURE_VARIABLES="${args_CONFIGURE_VARIABLES}"
            -DIN_PATH="${args_IN_PATH}"
            -DOUT_PATH="${args_OUT_PATH}"
            -P "${args_GENERATOR_MODULE_PATH}"
    )
endfunction()