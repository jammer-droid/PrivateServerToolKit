cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED PSTK_PACKET_CLI)
    message(FATAL_ERROR "PSTK_PACKET_CLI must name an already-built packet CLI executable")
endif()
if("${PSTK_PACKET_CLI}" STREQUAL "")
    message(FATAL_ERROR "PSTK_PACKET_CLI must not be empty")
endif()

# In cmake -P mode CMAKE_SOURCE_DIR is the caller's working directory.
get_filename_component(_caller_cwd "${CMAKE_SOURCE_DIR}" ABSOLUTE)
get_filename_component(_cli_path "${PSTK_PACKET_CLI}" ABSOLUTE BASE_DIR "${_caller_cwd}")
if(NOT EXISTS "${_cli_path}")
    message(FATAL_ERROR "PSTK_PACKET_CLI does not exist: ${_cli_path}")
endif()
if(IS_DIRECTORY "${_cli_path}")
    message(FATAL_ERROR "PSTK_PACKET_CLI must be a file, not a directory: ${_cli_path}")
endif()

# tests -> cli -> packet -> tools -> repository root
get_filename_component(_tests_dir "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(_cli_source_dir "${_tests_dir}" DIRECTORY)
get_filename_component(_packet_source_dir "${_cli_source_dir}" DIRECTORY)
get_filename_component(_tools_source_dir "${_packet_source_dir}" DIRECTORY)
get_filename_component(_repo_root "${_tools_source_dir}" DIRECTORY)

set(_schema_source_dir "${_tests_dir}/schemas")
if(NOT IS_DIRECTORY "${_schema_source_dir}")
    message(FATAL_ERROR "Packet CLI smoke schemas are missing: ${_schema_source_dir}")
endif()

set(_smoke_root "${_repo_root}/out/build/packet-cli-smoke")
file(MAKE_DIRECTORY "${_smoke_root}")
string(RANDOM LENGTH 12 ALPHABET "0123456789abcdef" _run_token)
set(_run_dir "${_smoke_root}/run-${_run_token}")
if(EXISTS "${_run_dir}")
    message(FATAL_ERROR "Packet CLI smoke run directory already exists: ${_run_dir}")
endif()
file(MAKE_DIRECTORY "${_run_dir}")
set(_schema_run_dir "${_run_dir}/schemas")
file(MAKE_DIRECTORY "${_schema_run_dir}/server")
file(COPY
    "${_schema_source_dir}/MovementInput.json"
    "${_schema_source_dir}/WorldTimeSyncRequest.json"
    DESTINATION "${_schema_run_dir}")
file(COPY "${_schema_source_dir}/server/WorldTimeSyncResponse.json" DESTINATION "${_schema_run_dir}/server")

function(pstk_packet_cli_smoke_fail case_name case_dir stdout stderr reason)
    message(FATAL_ERROR
        "Packet CLI smoke case '${case_name}' failed: ${reason}\n"
        "stdout:\n${stdout}\n"
        "stderr:\n${stderr}\n"
        "Artifacts retained at: ${case_dir}")
endfunction()

function(pstk_packet_cli_smoke_case case_name language input_value namespace_value)
    set(_case_dir "${_run_dir}/${case_name}")
    set(_ini_path "${_case_dir}/packet.ini")
    set(_output_dir "${_case_dir}/output")
    set(_working_dir "${_run_dir}/work/${case_name}")
    set(_expected_files ${ARGN})

    file(MAKE_DIRECTORY "${_case_dir}" "${_working_dir}")
    file(WRITE "${_ini_path}"
        "[packet]\n"
        "language=${language}\n"
        "input=${input_value}\n"
        "output=output\n"
        "namespace=${namespace_value}\n")

    execute_process(
        COMMAND "${_cli_path}" "${_ini_path}"
        WORKING_DIRECTORY "${_working_dir}"
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr
        TIMEOUT 30)
    if(NOT "${_result}" STREQUAL "0")
        pstk_packet_cli_smoke_fail(
            "${case_name}" "${_case_dir}" "${_stdout}" "${_stderr}"
            "CLI result was '${_result}'")
    endif()

    list(SORT _expected_files)
    file(GLOB _actual_entries LIST_DIRECTORIES true RELATIVE "${_output_dir}" "${_output_dir}/*")
    list(SORT _actual_entries)
    string(JOIN ", " _expected_text ${_expected_files})
    string(JOIN ", " _actual_text ${_actual_entries})
    if(NOT "${_actual_entries}" STREQUAL "${_expected_files}")
        pstk_packet_cli_smoke_fail(
            "${case_name}" "${_case_dir}" "${_stdout}" "${_stderr}"
            "output entries differed (expected: ${_expected_text}; actual: ${_actual_text})")
    endif()

    foreach(_entry IN LISTS _expected_files)
        file(READ "${_output_dir}/${_entry}" _contents)
        if(language STREQUAL "cpp")
            string(REPLACE ".generated.h" "" _packet_name "${_entry}")
            set(_declaration "struct ${_packet_name}")
            set(_declaration_pattern
                "(^|[^A-Za-z0-9_])struct[ ]+${_packet_name}([^A-Za-z0-9_]|$)")
        else()
            string(REPLACE ".generated.cs" "" _packet_name "${_entry}")
            set(_declaration "record struct ${_packet_name}")
            set(_declaration_pattern
                "(^|[^A-Za-z0-9_])record[ ]+struct[ ]+${_packet_name}([^A-Za-z0-9_]|$)")
        endif()

        string(REGEX MATCH "${_declaration_pattern}" _declaration_match "${_contents}")
        if("${_declaration_match}" STREQUAL "")
            pstk_packet_cli_smoke_fail(
                "${case_name}" "${_case_dir}" "${_stdout}" "${_stderr}"
                "${_entry} is missing DTO declaration '${_declaration}'")
        endif()

        if("${namespace_value}" STREQUAL "")
            set(_namespace_pattern
                "(^|[\r\n])[ \t]*namespace([ \t\r\n{]|$)")
            string(REGEX MATCH "${_namespace_pattern}" _namespace_match "${_contents}")
            if(NOT "${_namespace_match}" STREQUAL "")
                pstk_packet_cli_smoke_fail(
                    "${case_name}" "${_case_dir}" "${_stdout}" "${_stderr}"
                    "${_entry} unexpectedly contains a namespace declaration")
            endif()
        else()
            set(_namespace_pattern
                "(^|[\r\n])[ \t]*namespace[ \t]+${namespace_value}[ \t\r\n]*[{;]")
            string(REGEX MATCH "${_namespace_pattern}" _namespace_match "${_contents}")
            if("${_namespace_match}" STREQUAL "")
                pstk_packet_cli_smoke_fail(
                    "${case_name}" "${_case_dir}" "${_stdout}" "${_stderr}"
                    "${_entry} is missing namespace ${namespace_value}")
            endif()
        endif()
    endforeach()

    message(STATUS "[${case_name}] passed; artifacts retained at ${_case_dir}")
endfunction()

pstk_packet_cli_smoke_case(cpp-single cpp "../schemas/MovementInput.json" "CliSmoke" "MovementInput.generated.h")
pstk_packet_cli_smoke_case(csharp-single csharp "../schemas/MovementInput.json" "CliSmoke" "MovementInput.generated.cs")
pstk_packet_cli_smoke_case(cpp-directory cpp "../schemas" ""
    "MovementInput.generated.h;WorldTimeSyncRequest.generated.h;WorldTimeSyncResponse.generated.h")
pstk_packet_cli_smoke_case(csharp-directory csharp "../schemas" ""
    "MovementInput.generated.cs;WorldTimeSyncRequest.generated.cs;WorldTimeSyncResponse.generated.cs")

message(STATUS "Packet CLI smoke passed: four configuration cases")
message(STATUS "Artifacts retained at: ${_run_dir}")
