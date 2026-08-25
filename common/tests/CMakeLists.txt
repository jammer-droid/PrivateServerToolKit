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
)

add_test(
    NAME pstk.common.contract.cpp
    COMMAND pst_common_contract_cpp
)
