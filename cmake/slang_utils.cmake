include_guard(GLOBAL)

function(AddSlangShaderTarget shader_file shader_stages output_dir other_targets)
    set(ENTRYPOINT_SUFFIX "Main") # TODO: expose this if necessary
    get_filename_component(SHADER_NAME "${shader_file}" NAME_WE)

    set(CURR_SPV_OUTPUTS "")
    foreach(stage ${shader_stages})
        set(SPV_OUTPUT "${output_dir}/${SHADER_NAME}.${stage}.spv")
        set(ENTRYPOINT_NAME "${stage}${ENTRYPOINT_SUFFIX}")
        add_custom_command(
            OUTPUT "${SPV_OUTPUT}"
            DEPENDS "${shader_file}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${output_dir}"
            COMMAND "${SLANGC_EXECUTABLE}" "${shader_file}"
                    -target spirv
                    -profile spirv_1_4
                    -emit-spirv-directly
                    -fvk-use-entrypoint-name
                    -entry ${ENTRYPOINT_NAME}
                    -o "${SPV_OUTPUT}"
            COMMENT "Compiling Slang shader file '${shader_file}' for '${stage}' stage."
            VERBATIM
        )

        string(TOUPPER ${SHADER_NAME} SHADER_NAME_UPPER)
        string(TOUPPER ${stage} STAGE_UPPER)
        set(COMPILE_DEFINITION_NAME "NPTRACER_SHADER_${SHADER_NAME_UPPER}_${STAGE_UPPER}")
        
        foreach(tgt ${other_targets})
            target_compile_definitions(${tgt} PUBLIC "${COMPILE_DEFINITION_NAME}=\"${SPV_OUTPUT}\"" ) # escape quotes
        endforeach()

        list(APPEND CURR_SPV_OUTPUTS "${SPV_OUTPUT}")
    endforeach()
    set(CURR_SPV_OUTPUTS ${CURR_SPV_OUTPUTS} PARENT_SCOPE)
endfunction()

function(ParseShaderSpec shader_spec)
    string(REPLACE "|" ";" SPEC_PARTS ${shader_spec}) # separate using `|` character
    
    list(LENGTH SPEC_PARTS SPEC_LEN)
    math(EXPR STAGE_COUNT "${SPEC_LEN} - 1") # the stage count is all after 1st item 

    list(GET SPEC_PARTS 0 CURR_SHADER_FILE)
    list(SUBLIST SPEC_PARTS 1 ${STAGE_COUNT} CURR_SHADER_STAGES)

    # make available outside of function
    set(CURR_SHADER_FILE "${CURR_SHADER_FILE}" PARENT_SCOPE)
    set(CURR_SHADER_STAGES "${CURR_SHADER_STAGES}" PARENT_SCOPE)
endfunction()