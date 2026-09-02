cmake_minimum_required(VERSION 3.24)

project(
        PrivateServerToolKit
        VERSION 0.1.0
        DESCRIPTION "Cross-platform native tools for PrivateServer"
        LANGUAGES C CXX
)

# option ( <변수 이름> <설명> <기본값> )
option(
        PSTK_BUILD_PACKET_TOOL
        "Build the packet tool"
        ON
)

# ctest 실행 -> test 프레임워크 실행 -> 전체 테스트 결과 집계
include(CTest)

add_subdirectory(common)
add_subdirectory(runtime/service_host)
add_subdirectory(execution)

if (PSTK_BUILD_PACKET_TOOL)
    add_subdirectory(tools/packet)
endif ()
