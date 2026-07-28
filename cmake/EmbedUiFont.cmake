if(NOT DEFINED INPUT_FILE OR NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "INPUT_FILE and OUTPUT_FILE are required")
endif()

file(READ "${INPUT_FILE}" font_hex HEX)
string(LENGTH "${font_hex}" font_hex_length)

set(font_bytes "")
set(offset 0)
while(offset LESS font_hex_length)
    math(EXPR remaining "${font_hex_length} - ${offset}")
    if(remaining GREATER 64)
        set(chunk_length 64)
    else()
        set(chunk_length "${remaining}")
    endif()

    string(
        SUBSTRING
        "${font_hex}"
        "${offset}"
        "${chunk_length}"
        font_hex_chunk
    )
    string(
        REGEX REPLACE
        "([0-9a-f][0-9a-f])"
        "0x\\1,"
        font_byte_chunk
        "${font_hex_chunk}"
    )
    string(APPEND font_bytes "    ${font_byte_chunk}\n")
    math(EXPR offset "${offset} + ${chunk_length}")
endwhile()

get_filename_component(output_directory "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
file(
    WRITE
    "${OUTPUT_FILE}"
    "#include \"UiFontData.hpp\"\n\n"
    "namespace arrow::assets {\n\n"
    "const unsigned char kUiFontData[] = {\n"
    "${font_bytes}"
    "};\n\n"
    "const std::size_t kUiFontDataSize = sizeof(kUiFontData);\n\n"
    "} // namespace arrow::assets\n"
)
