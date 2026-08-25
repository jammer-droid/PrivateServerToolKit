# Fixed-layout codec은 정확한 payload 크기를 요구한다

Generated fixed-layout packet codec은 입력 또는 출력 byte view의 크기가 해당 packet의 `PayloadBytes`와 정확히 일치할 때만 Encode 또는 Decode를 수행한다. 더 작거나 큰 view는 접근 전에 거부하며, 호출자가 큰 buffer에서 정확한 packet 구간을 view로 만들어 전달한다.

`PayloadBytes`는 payload version과 선언된 모든 field를 포함하지만 NetworkRuntime의 6-byte transport header는 포함하지 않는다. 첫 vertical slice인 `MovementInput`의 `PayloadBytes`는 14다.
