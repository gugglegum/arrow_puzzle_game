foreach(required INPUT_FILE OUTPUT_FILE HEADER_NAME SYMBOL_NAME)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(READ "${INPUT_FILE}" data_hex HEX)
string(LENGTH "${data_hex}" data_length)
set(data_bytes "")
set(offset 0)
while(offset LESS data_length)
    math(EXPR remaining "${data_length} - ${offset}")
    if(remaining GREATER 64)
        set(chunk_length 64)
    else()
        set(chunk_length "${remaining}")
    endif()
    string(SUBSTRING "${data_hex}" "${offset}" "${chunk_length}" chunk)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," chunk_bytes "${chunk}")
    string(APPEND data_bytes "    ${chunk_bytes}\n")
    math(EXPR offset "${offset} + ${chunk_length}")
endwhile()

get_filename_component(output_directory "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
file(WRITE "${OUTPUT_FILE}"
    "#include \"${HEADER_NAME}\"\n\n"
    "namespace arrow::assets {\n\n"
    "const unsigned char ${SYMBOL_NAME}[] = {\n${data_bytes}};\n\n"
    "const std::size_t ${SYMBOL_NAME}Size = sizeof(${SYMBOL_NAME});\n\n"
    "} // namespace arrow::assets\n"
)
