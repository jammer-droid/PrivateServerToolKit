# test for C 11
add_executable(
    pstk_common_contract_c
    CommonContract.c
)

target_compile_features(
    pstk_common_contract_c
    PRIVATE
    c_std_11
)

target_link_libraries(
    pstk_common_contract_c
    PRIVATE
    PSTK::Common
)

add_test(
    NAME pstk.common.contract.c
    COMMAND pstk_common_contract_c
)

# test for C++ 17
# GTest 라이브러리 target 가져옴
find_package(GTest CONFIG REQUIRED)

# CMake에서 사용할 GoogleTest 함수
include(GoogleTest)

add_executable(
    pstk_common_contract_cpp
    CommonContract.cpp
)

target_compile_features(
    pstk_common_contract_cpp
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    pstk_common_contract_cpp
    PRIVATE
    PSTK::Common
    GTest::gtest_main # GoogleTest 실행 파일 링크 / main 제공
)

# GoogleTest의 TEST를 CTest에 개별 등록
gtest_discover_tests(
    pstk_common_contract_cpp
    TEST_PREFIX "pstk.common.contract.cpp."
)
