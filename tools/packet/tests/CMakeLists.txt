# 소스 파일로 실행 파일 target을 생성
add_executable(
    pstk_packet_smoke # 논리 target 이름
    PacketToolApiSmoke.cpp # 소스 파일 경로
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
