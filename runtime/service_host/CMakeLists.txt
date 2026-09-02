include(GenerateExportHeader)

add_library(
    pstk_service_host
    SHARED
    src/TkServiceHost.cpp
)

add_library(
    PSTK::ServiceHost
    ALIAS
    pstk_service_host
)

target_compile_features(
    pstk_service_host
    PUBLIC
    cxx_std_17
)

target_link_libraries(
    pstk_service_host
    PUBLIC
    PSTK::Common
)

target_include_directories(
    pstk_service_host
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>
)

set_target_properties(
    pstk_service_host
    PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN TRUE
)

generate_export_header(
    pstk_service_host
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/pstk_service_host_export.h"
    EXPORT_MACRO_NAME PSTK_SERVICE_HOST_API
)

install(
    TARGETS pstk_service_host
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(
    FILES
    include/pstk/service_host/TkServiceHost.h
    include/pstk/service_host/TkServiceBinding.hpp
    "${CMAKE_CURRENT_BINARY_DIR}/pstk_service_host_export.h"
    DESTINATION include/pstk/service_host
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
