if(NOT DEFINED YCOMP OR NOT DEFINED TEST_ROOT OR NOT DEFINED CMAKE_COMMAND)
    message(FATAL_ERROR "YCOMP, TEST_ROOT, and CMAKE_COMMAND are required")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")
file(WRITE "${TEST_ROOT}/input.txt" "source\n")
file(WRITE "${TEST_ROOT}/increment.cmake"
"if(EXISTS \"\${COUNT_FILE}\")\n"
"  file(READ \"\${COUNT_FILE}\" count)\n"
"else()\n"
"  set(count 0)\n"
"endif()\n"
"math(EXPR count \"\${count} + 1\")\n"
"file(WRITE \"\${COUNT_FILE}\" \"\${count}\")\n"
"file(WRITE result.txt \"built\")\n")

file(WRITE "${TEST_ROOT}/CompilFileY.compy"
"!$all > result.txt\n"
"result.txt > input.txt\n"
"\t\"${CMAKE_COMMAND}\" -DCOUNT_FILE=count.txt -P increment.cmake\n"
"clr >\n"
"\t\"${CMAKE_COMMAND}\" -E rm -f result.txt count.txt\n")

execute_process(COMMAND "${YCOMP}" WORKING_DIRECTORY "${TEST_ROOT}"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Initial build failed: ${output}\n${error}")
endif()
file(READ "${TEST_ROOT}/count.txt" count)
if(NOT count STREQUAL "1")
    message(FATAL_ERROR "Expected first build count 1, got '${count}'")
endif()

execute_process(COMMAND "${YCOMP}" WORKING_DIRECTORY "${TEST_ROOT}"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Second build failed: ${output}\n${error}")
endif()
file(READ "${TEST_ROOT}/count.txt" count)
if(NOT count STREQUAL "1")
    message(FATAL_ERROR "Expected up-to-date build to be skipped; count is '${count}'")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)
file(APPEND "${TEST_ROOT}/input.txt" "changed\n")
execute_process(COMMAND "${YCOMP}" WORKING_DIRECTORY "${TEST_ROOT}"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Rebuild after dependency change failed: ${output}\n${error}")
endif()
file(READ "${TEST_ROOT}/count.txt" count)
if(NOT count STREQUAL "2")
    message(FATAL_ERROR "Expected dependency change to rebuild; count is '${count}'")
endif()

execute_process(COMMAND "${YCOMP}" clr WORKING_DIRECTORY "${TEST_ROOT}"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Clean target failed: ${output}\n${error}")
endif()
if(EXISTS "${TEST_ROOT}/result.txt")
    message(FATAL_ERROR "Clean target did not remove result.txt")
endif()

message(STATUS "YComp integration test passed")
