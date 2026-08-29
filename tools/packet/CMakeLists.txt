include (GenerateExportHeader)

add_subdirectory(src/schema)
add_subdirectory(src/generator)

# SHARED 라이브리러 빌드
# CMake에서 자동으로 compiler define에
# pstk_packet_EXPORTS 같은 export macro를 같이 정의
add_library(
    pstk_packet
    SHARED # 동적 라이브러리
    src/TkPacketTool.cpp
)

add_library(
    PSTK::Packet # 다른 CMake target이 사용할 별칭
    ALIAS
    pstk_packet
)

target_compile_features(
    pstk_packet
    PUBLIC
    cxx_std_17
)

target_link_libraries(
    pstk_packet
    PUBLIC
    PSTK::Common
    PRIVATE
    pstk_packet_cpp_generator
    pstk_packet_csharp_generator
)

# public header와 생성된 export header 공개
# BUILD_INTERFACE
# - 현재 소스 저장소의 build tree 안에서 target을
#   사용할 때 적용되는 경로
# INSTALL_INTERFACE
# -  Packet DLL과 헤더를 설치한 외부 프로젝트가
#    사용할 때 적용되는 경로
target_include_directories(
    pstk_packet
    PUBLIC

    # CMAKE_CURRENT_SOURCE_DIR = packet의 CMakeLists.txt가 있는 경로
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>

    # CMAKE_CURRENT_BINARY_DIR = packet tool의 빌드 결과 디렉터리
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>

    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# DLL 내부 symbol 감추는 설정
# - 모든 C++ symbol을 기본적으로 외부에 공개하지 않음
# - inline 함수의 외부 symbol도 숨김
set_target_properties(
    pstk_packet
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN TRUE
)

# 선택한 public API symbol만 공개하기 위한
# 매크로 헤더를 빌드 과정에서 생성
generate_export_header(
    pstk_packet
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/pstk_packet_export.h"
    EXPORT_MACRO_NAME PSTK_PACKET_API
)


if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
