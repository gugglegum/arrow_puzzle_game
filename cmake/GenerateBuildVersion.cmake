if(NOT DEFINED SOURCE_DIR OR NOT DEFINED OUTPUT_FILE OR
   NOT DEFINED RELEASE_VERSION)
    message(FATAL_ERROR "SOURCE_DIR, OUTPUT_FILE and RELEASE_VERSION are required")
endif()

set(version_label "")
# Explicit local previews use the intended release version for screenshots,
# without creating a tag or changing ordinary development build labels.
if(RELEASE_PREVIEW)
    set(version_label "v${RELEASE_VERSION}")
endif()
# Requiring .git here avoids describing an unrelated parent repository when
# an unpacked source archive happens to be built inside another checkout.
if(version_label STREQUAL "" AND EXISTS "${SOURCE_DIR}/.git" AND EXISTS "${GIT_EXECUTABLE}")
    execute_process(
        COMMAND
            "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" describe
            --tags --long --always --abbrev=7
            --match "v[0-9]*.[0-9]*.[0-9]*" --exclude "*-*" --exclude "*+*"
        RESULT_VARIABLE git_result
        OUTPUT_VARIABLE git_description
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(git_result EQUAL 0)
        if(git_description MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)-([0-9]+)-g([0-9a-f]+)$")
            set(release "${CMAKE_MATCH_1}")
            set(commits "${CMAKE_MATCH_2}")
            set(revision "${CMAKE_MATCH_3}")
            set(version_label "v${release}")
            if(NOT commits STREQUAL "0")
                string(APPEND version_label "+${commits} (${revision})")
            endif()
        elseif(git_description MATCHES "^[0-9a-f]+$")
            # A shallow checkout may not contain a release tag or its history.
            set(version_label "v${RELEASE_VERSION}+dev (${git_description})")
        endif()
    endif()
endif()

if(version_label STREQUAL "")
    # Source archives and builds without Git still have a useful identifier.
    string(TIMESTAMP build_time "%Y-%m-%d %H:%M UTC" UTC)
    set(version_label "v${RELEASE_VERSION}+local (${build_time})")
endif()

set(header "#pragma once\n\nnamespace arrow::build {\n\ninline constexpr char kVersionLabel[] = \"${version_label}\";\n\n} // namespace arrow::build\n")
set(previous_header "")
if(EXISTS "${OUTPUT_FILE}")
    file(READ "${OUTPUT_FILE}" previous_header)
endif()
if(NOT header STREQUAL previous_header)
    get_filename_component(output_directory "${OUTPUT_FILE}" DIRECTORY)
    file(MAKE_DIRECTORY "${output_directory}")
    file(WRITE "${OUTPUT_FILE}" "${header}")
endif()
