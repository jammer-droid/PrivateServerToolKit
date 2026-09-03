# src/common/CMakeLists.txt

add_library(
    pstk_common
    INTERFACE
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

install(
    FILES
        include/pstk/TkByteView.h
        include/pstk/TkDiagnostic.h
        include/pstk/TkResult.h
    DESTINATION include/pstk
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
