# common/CMakeLists.txt

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
