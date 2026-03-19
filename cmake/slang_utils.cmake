function (add_slang_shader_target TARGET)
	cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
	set (SHADERS_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
	set(SPV_OUTPUT "${SHADERS_DIR}/slang.spv")
	set (ENTRY_POINTS -entry vertMain -entry fragMain)
	add_custom_command (
		OUTPUT ${SPV_OUTPUT}
		COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADERS_DIR}"
		COMMAND "${SLANGC_EXECUTABLE}" ${SHADER_SOURCES}
                -target spirv
                -profile spirv_1_4
                -emit-spirv-directly
                -fvk-use-entrypoint-name
                ${ENTRY_POINTS}
                -o "${SPV_OUTPUT}"
		DEPENDS ${SHADER_SOURCES}
		COMMENT "Compiling Slang Shaders"
        VERBATIM
	)
	add_custom_target (${TARGET} DEPENDS "${SPV_OUTPUT}")
endfunction()