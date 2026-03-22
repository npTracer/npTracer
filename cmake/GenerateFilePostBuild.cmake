file(READ "${IN_PATH}" FILE_CONTENTS)

separate_arguments(CONFIGURE_VARIABLES) # split into list by spaces

foreach(var ${CONFIGURE_VARIABLES})
    # search for `=` to split VAR=VALUE
    string(REGEX MATCH "^([^=]+)=(.*)$" _ "${var}")

    set(KEY "${CMAKE_MATCH_1}")
    set(VAL "${CMAKE_MATCH_2}")

    message(STATUS "${KEY}")

    # replace @VAR@ with value
    string(REPLACE "@${KEY}@" "${VAL}" FILE_CONTENTS "${FILE_CONTENTS}")
endforeach()

file(WRITE "${OUT_PATH}" "${FILE_CONTENTS}")

message(STATUS "Configured input file '${IN_PATH}' with variables '${CONFIGURE_VARIABLES}' and output to '${OUT_PATH}'.")