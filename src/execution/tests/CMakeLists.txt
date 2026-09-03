find_package(GTest CONFIG REQUIRED)
# CMake가 운영체제의 스레드 지원 방식을 찾아주는 내장 패키지
find_package(Threads REQUIRED)

include(GoogleTest)

add_executable(
    pstk_execution_tests
    ExecutionTests.cpp
    TkSerialWorkLaneTests.cpp
    TkWorkerSchedulerTests.cpp
    TkWorkerPoolTests.cpp
)

target_compile_features(
    pstk_execution_tests
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    pstk_execution_tests
    PRIVATE
    PSTK::Execution
    GTest::gtest_main
)

gtest_discover_tests(
    pstk_execution_tests
    TEST_PREFIX "pstk.execution."
)
