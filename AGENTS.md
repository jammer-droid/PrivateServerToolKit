## Agent skills

### Issue tracker

이 저장소는 GitHub Issues를 사용하며 외부 PR은 트리아지 대상에 포함하지 않는다. 자세한 내용은 `docs/agents/issue-tracker.md`를 참고한다.

### Design docs

구현 계획과 grilling을 통해 확정한 issue 전용 design은 GitHub Issue 단위로 `docs/design/issue-<number>-<slug>.md`에 기록한다. Phase나 하위 slice를 별도 계획 문서로 만들지 않고 해당 issue 문서의 section으로 관리한다. 자세한 workflow는 `docs/agents/design-workflow.md`를 참고한다.

### Triage labels

상태 라벨은 `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`를 사용한다. 자세한 매핑은 `docs/agents/triage-labels.md`를 참고한다.

### Domain docs

단일 컨텍스트 구성으로 루트 `CONTEXT.md`와 `docs/adr/`를 사용한다. 자세한 내용은 `docs/agents/domain.md`를 참고한다.

### C++ API contracts

C++의 성공/실패 반환은 공용 `TkResult`로 통일하며 모듈별 result 타입을 추가하지 않는다. Predicate, diagnostic, 실패 시 output 보존을 구분하는 기준은 `docs/agents/cpp-result-style.md`를 참고한다.
