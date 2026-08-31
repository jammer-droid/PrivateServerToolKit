# ToolKit 서버 런타임 확장 브레인스토밍

현재 논의한 목표와 책임 구분을 보관하는 메모다. 상세 설계, 구현 계획이나 Issue 등록을 위한 명세는 아니며, 아래 모듈 이름과 구체적인 경계는 후속 논의에서 다듬는다.

## 목표

Packet Tool과 Service Host에 더해 NetworkRuntime과 WorldRuntime까지 ToolKit에서 제공하는 것을 목표로 한다. 게임 프로젝트가 통신·호출·World 실행 기반을 반복 구현하지 않고, 게임별 schema, service handler, ECS component/system과 실행 설정에 집중할 수 있게 한다.

## 크게 나눠볼 구성

```text
+------------------------+     +-------------------------------------+
| Network Runtime        |     | Supporting Tools / Libraries        |
+------------------------+     |                                     |
             ^                 | Packet Tool (build-time)            |
             |                 |   schema -> DTO / codec             |
             v                 |                                     |
+------------------------+     | Common Libraries / Helpers          |
| Service Host           |     |   TkResult / Diagnostic             |
+------------------------+     |   Byte View / codec support         |
             ^                 |   Other reusable helpers            |
             |                 +-------------------------------------+
             v
+------------------------+
| WorldRuntime           |
| Tick / ECS execution   |
+------------------------+
```

세로축은 adapter를 통한 런타임 간 입력·결과 흐름이며 직접적인 구현 의존성을 뜻하지 않는다. 오른쪽은 필요한 계층에서 사용하는 지원 구성요소다. Packet Tool은 실행 경로에 들어가는 런타임 계층이 아니라 DTO와 codec을 생성하는 빌드 시점 도구다.

| 구성 | 생각하고 있는 역할 |
| --- | --- |
| Packet Tool | 공통 schema에서 DTO와 codec을 생성하는 빌드 시점 도구 |
| NetworkRuntime | 연결과 송수신, framing, session과 buffer 수명을 관리하는 통신 기반 |
| Service Host | generated Decode, middleware, typed handler 연결과 비동기 호출의 완료·응답 관리 |
| WorldRuntime | 입력 수집, tick, ECS system 실행, 상태 변경 확정과 결과 출력을 관리하는 재사용 실행 기반 |
| 게임 프로젝트 | 게임별 handler, component/system과 규칙을 작성하고 공통 런타임을 조립하는 실행 프로그램 |

World Server는 공통 런타임과 게임 로직을 조립한 애플리케이션으로 보고, 그 안에서 재사용할 실행 기반을 잠정적으로 WorldRuntime이라 부른다.

## 사용 모습과 책임 경계

- 초기화 지점에서 게임 서비스, middleware, 실행 adapter와 ECS system을 등록하고 서버를 실행하는 사용 경험을 지향한다. 범용 DI container나 동적 플러그인 로딩을 채택한다는 뜻은 아니다.
- Service Host는 통신 입력을 서비스 호출로 연결한다. payload의 기초 검증은 generated codec을 사용하며, 게임 규칙 검증은 게임 로직에 남긴다.
- WorldRuntime은 입력을 언제, 어떤 순서와 실행 영역에서 처리할지 소유한다. double buffer로 입력·명령을 모아 tick에서 처리하는 방식은 가능한 예시이지 확정된 구현이 아니다.
- Service Host는 ECS나 World 내부 버퍼에 직접 의존하지 않고 adapter를 통해 연결한다. 큐 접수와 실제 게임 처리 완료는 구분한다.
- 이 목표는 실시간 게임 서버의 공통 실행 기반이다. DB, 인증, 매치메이킹과 분산 운영까지 모두 제공하는 완성형 백엔드를 뜻하지는 않는다.

## 이후에 따로 결정할 것

- WorldRuntime이 제공할 ECS 기능과 게임별 component/system의 경계, tick·입력·상태 변경·출력 계약
- Service Host와 WorldRuntime 사이의 handler 실행 위치, 데이터 소유권과 완료 전달 방식
- 기존 NetworkRuntime의 재사용·이관 범위: Windows/IOCP 지원, 기존 공개 ABI와 ToolKit 공통 계약의 연결, build·의존성 및 수명 계약 재검증
- 공개 API, 배포 단위와 플랫폼 범위, 구체적인 구현 순서

현재 ToolKit의 플랫폼 독립 native tool 중심 범위에서 실행 런타임까지 확장하는 방향이다. NetworkRuntime 이관이 코딩 스타일 변경만으로 끝난다고 가정하지 않는다.

[Issue #2의 Service Host 설계](../issue-2-typed-service-host.md)는 기존 범위를 유지한다. WorldRuntime과 NetworkRuntime 이관은 별도의 후속 주제로 남기며, 이 메모만으로 Issue 범위나 구현 권한을 확대하지 않는다.

- https://github.com/SanderMertens/flecs
