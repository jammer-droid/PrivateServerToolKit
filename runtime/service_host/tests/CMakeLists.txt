find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(
    pstk_service_host_tests
    TkServiceHostTests.cpp
    TkServiceHostPipelineTests.cpp
    TkServiceHostResponseTests.cpp
    TkServiceBindingTests.cpp
)

target_compile_features(
    pstk_service_host_tests
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    pstk_service_host_tests
    PRIVATE
    PSTK::ServiceHost
    GTest::gtest_main
)

gtest_discover_tests(
    pstk_service_host_tests
    TEST_PREFIX "pstk.service-host."
)

add_executable(
    pstk_service_host_smoke
    TkServiceHostSmoke.cpp
)

target_compile_features(
    pstk_service_host_smoke
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    pstk_service_host_smoke
    PRIVATE
    PSTK::ServiceHost
)

add_test(
    NAME pstk.service-host.api.smoke
    COMMAND pstk_service_host_smoke
)
