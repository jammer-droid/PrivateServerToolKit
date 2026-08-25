# 버퍼 접근에 byte view를 사용한다

PrivateServerToolKit의 C++17 공용 계약은 `std::span` 호환 타입이나 generic element view를 구현하지 않는다. 읽기 전용 `TkByteView`는 `const uint8_t*`와 `size_t`, 쓰기 가능한 `TkMutableByteView`는 `uint8_t*`와 `size_t`를 보관하는 별도 non-owning POD로 정의하여 C와 C++에서 같은 계약을 사용한다.

두 view는 메모리 lifetime을 관리하거나 범위를 소유하지 않는다. 읽기와 쓰기 타입을 분리하여 packet decode 입력과 encode 출력을 const 계약으로 구분한다.

`size == 0`이면 `data` 값과 관계없이 유효한 empty view다. `size > 0`인 view는 `data != nullptr`이어야 하며, 양수 크기와 null 주소의 조합은 유효하지 않다.

Common 계층은 이 구조적 유효성을 검사하는 `TkIsValidByteRange(const void* data, size_t size)`를 header-only 함수로 제공한다. 이 함수는 lifetime, 실제 메모리 용량 또는 packet별 요구 크기를 검사하지 않으며, 해당 조건은 view를 소비하는 API가 검증한다. Common 계층은 이 계약을 위해 별도 binary 의존성을 추가하지 않는다.
