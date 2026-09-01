find_package(Threads REQUIRED)

add_library(
    pstk_execution
    STATIC
    src/TkWorkItem.cpp
    src/TkWorkerPool.cpp
)

add_library(
    PSTK::Execution
    ALIAS
    pstk_execution
)

target_compile_features(
    pstk_execution
    PUBLIC
    cxx_std_17
)

target_include_directories(
    pstk_execution
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(
    pstk_execution
    PUBLIC
    PSTK::Common
    Threads::Threads
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
