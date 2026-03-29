include_guard(GLOBAL)

function(AddSlangShaderTarget compile_target_name shader_file shader_stages output_dir other_targets)
    set(ENTRYPOINT_SUFFIX "Main")
    get_filename_component(SHADER_NAME "${shader_file}" NAME_WE)

    foreach(stage IN LISTS shader_stages)
        set(SPV_OUTPUT "${output_dir}/${SHADER_NAME}.${stage}.spv")
        set(ENTRYPOINT_NAME "${stage}${ENTRYPOINT_SUFFIX}")

        # Incremental build rule
        add_custom_command(
            OUTPUT "${SPV_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${output_dir}"
            COMMAND "${SLANGC_EXECUTABLE}" "${shader_file}"
                    -target spirv
                    -profile spirv_1_5
                    -emit-spirv-directly
                    -fvk-use-entrypoint-name
                    -entry "${ENTRYPOINT_NAME}"
                    -o "${SPV_OUTPUT}"
            DEPENDS "${shader_file}"
            COMMENT "Compiling Slang shader file '${shader_file}' for '${stage}' stage."
            VERBATIM
        )

        # Collect outputs for the incremental target
        set_property(GLOBAL APPEND PROPERTY ALL_SPV_OUTPUTS "${SPV_OUTPUT}")

        # Collect commands for the manual target
        set_property(GLOBAL APPEND PROPERTY ALL_SHADER_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E make_directory "${output_dir}"
            COMMAND "${SLANGC_EXECUTABLE}" "${shader_file}"
                    -target spirv
                    -profile spirv_1_5
                    -emit-spirv-directly
                    -fvk-use-entrypoint-name
                    -entry "${ENTRYPOINT_NAME}"
                    -o "${SPV_OUTPUT}"
        )

        string(TOUPPER "${SHADER_NAME}" SHADER_NAME_UPPER)
        string(TOUPPER "${stage}" STAGE_UPPER)
        set(COMPILE_DEFINITION_NAME "NPTRACER_SHADER_${SHADER_NAME_UPPER}_${STAGE_UPPER}")

        foreach(tgt IN LISTS other_targets)
            target_compile_definitions(${tgt}
                PUBLIC
                "${COMPILE_DEFINITION_NAME}=\"${SPV_OUTPUT}\""
            )
        endforeach()
    endforeach()
endfunction()

function(ParseShaderSpec shader_spec)
    string(REPLACE "|" ";" SPEC_PARTS "${shader_spec}")

    list(LENGTH SPEC_PARTS SPEC_LEN)
    math(EXPR STAGE_COUNT "${SPEC_LEN} - 1")

    list(GET SPEC_PARTS 0 CURR_SHADER_FILE)
    list(SUBLIST SPEC_PARTS 1 ${STAGE_COUNT} CURR_SHADER_STAGES)

    set(CURR_SHADER_FILE "${CURR_SHADER_FILE}" PARENT_SCOPE)
    set(CURR_SHADER_STAGES "${CURR_SHADER_STAGES}" PARENT_SCOPE)
endfunction()