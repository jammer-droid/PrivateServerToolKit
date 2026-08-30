# Generated code의 소비자 의존성을 언어별로 구분한다

Packet source를 생성하는 compiler API consumer와 생성된 packet을 사용하는 runtime consumer는 서로 다른 계약을 가진다. C++은 ToolKit의 주 대상이지만 다른 언어의 packet consumer에게까지 native 공통 계층을 강제하면 패킷을 사용하기 위해 불필요한 ToolKit 의존성과 언어별 binding을 관리해야 한다. 따라서 wire 계약은 공유하되 generated-code API와 의존성은 대상 언어에 맞게 구분한다.

- Compiler API consumer는 생성 대상 언어와 관계없이 ToolKit의 C ABI, `TkPacketCompileInfo`, `TkResult`와 Diagnostic 계약을 사용한다.
- Generated C++ code는 ToolKit의 공통 public header와 C++ codec support를 사용하며 `TkResult`, byte view와 Diagnostic 계약을 유지한다. SDK header 의존성이 codec 사용만을 위한 native shared library 링크를 뜻하지는 않는다. 실제 compiler API를 호출하는 경계의 shared library 의존성과 구분한다.
- Generated non-C++ code는 ToolKit native shared library, 공통 header, `TkResult`와 `TkDiagnostic`을 요구하지 않는다. 이 공통 타입을 해당 언어로 복제해 소비자 계약으로 강제하지 않으며, 필요한 codec support는 해당 언어의 코드로 제공할 수 있다.

언어 중립적으로 공유하는 것은 packet ID, payload version, payload size, field layout과 wire bytes다. 생성 source 자체나 성공·실패 API의 형태를 언어 중립적으로 통일하지 않는다. 현재 C#은 C# codec support와 함께 컴파일되는 `TryEncode`/`TryDecode`를 사용하며, encode 실패 시 buffer를 보존하고 decode 실패 시 `false`와 `default`를 반환한다.

이 결정은 [Issue #1의 consumer 경계](../design/issue-1-packet-codegen-rpc.md)에 확정한 계약을 후속 언어와 consumer integration에도 적용하기 위해 기록한다. SDK의 구체적인 배포와 build hook은 별도 구현 계약이며 이 ADR로 배포 완료를 의미하지 않는다.
