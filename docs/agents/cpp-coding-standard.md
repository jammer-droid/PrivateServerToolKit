# PrivateServerToolKit C++ project rules

## 범위와 우선순위

이 저장소는 `agent-context`가 설치하는 전역 C++ coding standard와 shared-library coding standard를 기본값으로 사용한다. 이 문서는 공통 규약을 복제하지 않고 PrivateServerToolKit에서 달라지거나 더 구체적인 계약만 기록한다.

충돌 시 다음 순서로 판단한다.

1. 공개된 C ABI, wire/schema contract와 accepted ADR
2. `docs/agents/cpp-result-style.md`와 이 문서의 프로젝트 규칙
3. 전역 C++ 및 shared-library coding standard
4. 주변 코드의 일관된 기존 형식

새로 작성하거나 실질적으로 수정하는 handwritten C++부터 적용한다. 규약 적용만을 위한 기존 코드 일괄 rename, reformat 또는 ABI 변경은 하지 않는다.

## C-compatible public contract

`common` header와 Packet Tool의 공개 경계는 C와 C++ consumer가 함께 사용하는 C ABI다.

- `typedef struct`, `typedef enum`, lowerCamelCase field와 이미 공개된 symbol 및 numeric value를 유지한다.
- 공개 계약을 `enum class`, reference parameter, constructor 또는 C++ member function으로 바꾸지 않는다.
- `extern "C"`는 C++ name mangling을 제거해야 하는 exported function declaration에 사용한다. 단순 type declaration 자체에는 필요하지 않다.
- 공개 signature와 object layout에는 STL type, exception, RTTI 또는 compiler-dependent C++ class layout을 노출하지 않는다.
- Export macro는 DLL 밖에서 호출해야 하는 symbol에만 붙이고 나머지 symbol은 기본적으로 숨긴다.
- 한 module에서 할당한 메모리는 같은 module이 해제한다. Module이 ownership을 넘긴다면 대응하는 destroy/free API를 같은 경계에 제공한다.

C translation unit에서 필요한 `NULL`과 C header 표현은 C++ 스타일로 기계적으로 바꾸지 않는다.

## Result, diagnostic과 assertion

- 성공과 실패는 public/internal 모두 공용 `TkResult`로 전달하고 layer나 tool마다 별도 result type을 만들지 않는다.
- `TkResult`는 control-flow 분류로 유지하고 source, stable ID와 message 같은 상세 정보는 common Diagnostic 계약으로 분리한다.
- `TkResult`를 반환하는 함수는 별도 명시가 없으면 실패 시 caller의 output object와 buffer를 변경하지 않는다.
- Consumer 입력, null output, schema 오류와 I/O 실패는 `TkResult`로 처리하고 assertion으로 대체하지 않는다.
- Assertion은 tool 내부에서 이미 성립해야 하는 invariant에만 사용하며 표현식에 필수 side effect를 넣지 않는다. Compile-time 조건은 `static_assert`를 사용한다.
- Diagnostic callback의 lifetime, 호출 thread와 concurrency 계약은 `docs/adr/0004-use-common-diagnostic-callbacks.md`를 따른다.

## Buffer view와 ownership

- 읽기 전용 `TkByteView`와 쓰기 가능한 `TkMutableByteView`는 pointer와 size만 가진 non-owning POD다.
- Empty view에서는 null pointer를 허용하지만 non-empty view는 유효한 범위를 가리켜야 한다.
- View는 storage lifetime이나 capacity를 소유하거나 연장하지 않는다.
- Callback, `userData`와 borrowed diagnostic pointer는 해당 계약이 허용한 lifetime을 넘겨 저장하지 않는다.

## Include, formatting과 generated code

- Formatting은 저장소의 `.clang-format`을 따르고 기존 include group 순서를 보존한다.
- 설치 가능한 project public header는 `#include <pstk/...>` 형식을 사용하고 같은 component의 private header는 quote include를 사용할 수 있다.
- 현재 handwritten tool 구현의 최소 언어 버전은 C++17이다. C-compatible common contract는 C11 translation unit에서도 compile 가능해야 한다.
- Generated source는 issue design의 public contract, schema field 이름과 deterministic formatting을 우선한다.
- Generated DTO의 member `Encode`/`Decode`와 schema-derived lowerCamelCase field는 handwritten type 규칙의 예외다.
- Generated source는 사람이 직접 수정하거나 스타일만을 위해 후처리하지 않는다.

## 프로젝트 근거

- `docs/agents/cpp-result-style.md`
- `docs/adr/0001-use-byte-views-for-buffer-access.md`
- `docs/adr/0003-use-tkresult-for-cpp-failures.md`
- `docs/adr/0004-use-common-diagnostic-callbacks.md`
- `docs/cpp/SharedLibraryBinaryModel.md`
- `docs/cmake/PacketTarget.md`
