cmake_minimum_required(VERSION 3.24)

project(
        PrivateServerToolKit
        VERSION 0.1.0
        DESCRIPTION "Cross-platform native tools for PrivateServer"
        LANGUAGES CXX
)

# option ( <변수 이름> <설명> <기본값> )
option(
        PSTK_BUILD_PACKET_TOOL
        "Build the packet tool"
        ON
)

# ctest 실행 -> test 프레임워크 실행 -> 전체 테스트 결과 집계
include(CTest)

add_library(
    pstk_common
    INTERFACE # 공용 public header 경로 전달 용도, binary 생성 없음
)

add_library(
    PSTK::Common
    ALIAS
    pstk_common
)

target_include_directories(
    pstk_common
    INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if (PSTK_BUILD_PACKET_TOOL)
    add_subdirectory(tools/packet)
endif ()
