# 소스 파일로 실행 파일 target을 생성
add_executable(
    pstk_packet_smoke # 논리 target 이름
    TkPacketToolSmoke.cpp # 소스 파일 경로
)

# 링커가 사용할 라이브러리 지정
target_link_libraries(
    pstk_packet_smoke
    PRIVATE
    PSTK::Packet # Packet target alias 사용
)

# executable target을 CTest에 등록
add_test(
    NAME pstk.packet.api.version # CTest의 테스트 식별자

    # add_executable로 정의된 executable target을
    # CMake에서 실제 실행 파일 경로로 전달
    COMMAND pstk_packet_smoke
)

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(
    pstk_packet_tests
    TkPacketCompilerTests.cpp
)

target_compile_features(
    pstk_packet_tests
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    pstk_packet_tests
    PRIVATE
    PSTK::Packet
    GTest::gtest_main
)

gtest_discover_tests(
    pstk_packet_tests
    TEST_PREFIX "pstk.packet.compiler."
)

add_executable(
    pstk_packet_schema_compiler_tests
    TkPacketSchemaCompilerTests.cpp
)

target_compile_features(
    pstk_packet_schema_compiler_tests
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    pstk_packet_schema_compiler_tests
    PRIVATE
    pstk_packet_schema_compiler
    GTest::gtest_main
)

gtest_discover_tests(
    pstk_packet_schema_compiler_tests
    TEST_PREFIX "pstk.packet.schema."
)
