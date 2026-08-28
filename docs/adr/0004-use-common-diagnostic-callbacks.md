# 모든 tool은 common Diagnostic Callback Info를 사용한다

PrivateServerToolKit의 tool은 `severity`, 안정적인 문자열 `id`, 사람이 읽는 `message`, optional source `location`을 가진 공용 `TkDiagnostic`을 사용한다. Source location은 `sourceName`, `byteOffset`, `line`, `column`으로 구성하며 다음 세 상태를 사용한다.

- source가 없으면 `sourceName == nullptr`이고 `byteOffset`, `line`, `column`은 모두 0이다.
- source는 알지만 정확한 위치를 모르면 `sourceName != nullptr`이고 `byteOffset`, `line`, `column`은 모두 0이다.
- 정확한 위치를 알면 `sourceName != nullptr`, `byteOffset`은 UTF-8 source 시작부터 계산한 0-based byte offset, `line`과 `column`은 1-based다. `column`도 Unicode code point 수가 아니라 현재 line 시작부터 계산한 byte 위치를 사용한다.

`(byteOffset, line, column) == (0, 0, 0)`은 unknown-location sentinel이며, source 시작 위치는 `(0, 1, 1)`이므로 충돌하지 않는다. `(0, 0, 0)`이 아닌 모든 위치 tuple은 정확한 위치 형식이어야 하며 `line`과 `column`은 각각 1 이상이어야 한다. 따라서 0이 아닌 `byteOffset`과 0인 `line` 또는 `column`을 포함한 혼합 상태와 그 밖의 부분적인 위치 표현은 사용하지 않는다.

Diagnostic은 `TkDiagnosticCallbackInfo`의 synchronous callback과 `userData`를 통해 전달한다. Diagnostic과 그 문자열은 callback 동안만 유효한 borrowed data이며, 보관하려는 consumer가 복사한다.

`TkDiagnosticCallbackInfo::callback == nullptr`은 유효한 disabled callback이며 diagnostic을 버리고 `userData`를 사용하지 않는다. Callback이 있으면 해당 tool API를 호출한 thread에서 API가 반환되기 전에 순차적으로 실행하며, observer 등록 여부는 tool의 `TkResult`를 바꾸지 않는다.

Tool은 callback info, `userData` 또는 Diagnostic의 borrowed pointer를 API 반환 이후 보관하지 않는다. 한 API 호출 안에서 callback을 동시에 실행하지 않지만, 서로 다른 thread가 같은 callback info를 사용하여 tool API를 동시에 호출하면 callback 실행이 겹칠 수 있으며 이 경우 consumer가 `userData` 접근을 동기화한다.

각 tool은 `PSTK-PACKET-UNKNOWN-TYPE`처럼 tool prefix를 포함하는 안정적인 ID를 정의하되 별도 diagnostic 포맷이나 callback 타입을 만들지 않는다. Generic key-value context와 확장 chain은 실제 요구가 생기기 전까지 추가하지 않는다.

Diagnostic `id`는 non-null, non-empty ASCII이며 `PSTK-<TOOL>-<NAME>` 형식을 사용하고 한번 공개된 의미를 변경하지 않는다. `message`는 non-null, non-empty UTF-8 설명이지만 안정적인 parsing 대상은 아니다. Optional `sourceName`은 non-null일 때 non-empty UTF-8이어야 한다.

Common 계층은 Diagnostic과 Callback Info의 POD, callback function pointer type과 C-compatible header-only `TkEmitDiagnostic` helper를 제공한다. `TkEmitDiagnostic`은 non-null Diagnostic을 borrowed data로 받아 callback이 disabled인지 확인하고, 활성 callback을 `userData`와 함께 동기 호출하는 동작만 소유한다. Diagnostic을 구성하고 ID, message, location, emit 시점과 순서 및 `TkResult`를 정하는 구현은 각 tool이 소유하고, 저장, 출력, 필터링과 동기화는 consumer가 소유한다. Common은 formatting, 저장소, logger, callback registry, allocation 또는 thread를 소유하지 않는다.
