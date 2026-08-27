## Agent skills

### Issue tracker

이 저장소는 GitHub Issues를 사용하며 외부 PR은 트리아지 대상에 포함하지 않는다. 자세한 내용은 `docs/agents/issue-tracker.md`를 참고한다.

여러 세션이 필요한 큰 작업의 경로가 불명확하면 `$wayfinder`로 decision map을 만들고, 오래된 backlog issue를 다시 검토할 때는 `$triage`를 사용한다.

### Design docs

구현 계획과 `$refine-issue`로 확정한 issue 전용 design은 GitHub Issue 단위로 `docs/design/issue-<number>-<slug>.md`에 기록한다. Phase나 하위 slice를 별도 계획 문서로 만들지 않고 해당 issue 문서의 stable-ID section으로 관리한다. 자세한 workflow는 `docs/agents/design-workflow.md`를 참고한다.

### Triage labels

상태 라벨은 `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`를 사용한다. `ready-for-agent`는 다음 agent workflow 추천이지 구현 권한 자체가 아니다. 자세한 매핑은 `docs/agents/triage-labels.md`를 참고한다.

### Domain docs

단일 컨텍스트 구성으로 루트 `CONTEXT.md`와 `docs/adr/`를 사용한다. 자세한 내용은 `docs/agents/domain.md`를 참고한다.

### C++ API contracts

C++의 성공/실패 반환은 공용 `TkResult`로 통일하며 모듈별 result 타입을 추가하지 않는다. Predicate, diagnostic, 실패 시 output 보존을 구분하는 기준은 `docs/agents/cpp-result-style.md`를 참고한다.

### C++ coding standard

Handwritten C++은 `agent-context`의 전역 C++ coding standard를 기본값으로 사용하고, DLL/shared library 경계 작업에는 전역 shared-library standard도 함께 적용한다. 기존 C ABI, accepted ADR, wire/generated 계약과 이 저장소의 예외가 전역 기본값보다 우선한다. 프로젝트별 적용 기준은 `docs/agents/cpp-coding-standard.md`를 참고한다.

### Lean implementation workflow

구현 가이드 요청과 일반 C/C++ 구현 작업은 `$lean-implementation`을 사용한다. Guide가 기본이며 명시적 변경 요청만 구현을 위임한다. Test-first는 별도 workflow가 아니라 suitability gate를 만족한 slice에서만 사용한다.
