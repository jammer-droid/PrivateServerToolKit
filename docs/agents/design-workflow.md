# GitHub Issue design workflow

## 문서 단위와 경로

구현 계획과 `$refine-issue`를 통해 확정한 상세 design의 단위는 GitHub Issue다. 문서는 다음 경로에 issue당 하나로 유지한다.

```text
docs/design/issue-<number>-<slug>.md
```

Issue 제목이 바뀌어도 기존 문서의 number와 경로를 불필요하게 바꾸지 않는다. Phase, milestone, vertical slice 또는 하위 계약은 별도 계획 문서로 분리하지 않고 해당 issue 문서의 section으로 추가한다.

## Source of truth

- GitHub Issue: 목표, 범위, 상태, dependency와 상위 완료 조건
- Issue design 문서: 세부 계약, 선택 근거, 구현 순서, Phase/slice, 검증 계획과 미해결 결정
- `CONTEXT.md`: 여러 issue에서 공유하는 domain 용어
- `docs/adr/`: 여러 issue에 영향을 주고 되돌리기 어려운 구조적 결정
- Code/test: 현재 구현과 검증 상태

Issue design 문서는 issue 본문을 전체 복사하지 않고 상세 design에 필요한 범위와 제약만 요약한다. Issue와 문서가 충돌하면 임의로 한쪽을 덮어쓰지 않고 차이를 먼저 드러낸다.

## 작업 순서

1. `gh issue view <number> --repo jammer-droid/PrivateServerToolKit`으로 issue 제목, 범위, 상태와 완료 조건을 확인한다.
2. 같은 number의 `docs/design/issue-<number>-*.md`가 있는지 확인하고 있으면 새 문서 대신 기존 문서를 갱신한다.
3. `CONTEXT.md`와 관련 ADR, code/test에서 현재 계약과 사실을 확인한다.
4. 구현 결과를 바꾸는 물질적 결정을 `$refine-issue`의 grilling 단계에서 한 번에 하나씩 확정한다.
5. 확정된 결정과 complete dependency-ordered stable-ID slice plan을 해당 issue 문서에 반영하고, 미해결 항목은 결정된 것처럼 작성하지 않는다.
6. 공유 용어나 cross-issue 구조 결정이 생기면 `CONTEXT.md` 또는 ADR을 갱신하고 issue 문서에서 연결한다.
7. 문서 링크, `git diff --check`, issue 범위와 완료 기준을 검증한다.

Issue 생성, 본문·라벨·관계 변경, comment와 close는 publish다. 로컬 design 문서 작성이 허용되어도 GitHub 원격 상태를 자동으로 변경하지 않는다.

## Readiness gate와 slice contract

Issue design은 다음 항목이 모두 있어야 구현 준비가 끝난다.

- 목표, 포함 범위와 제외 범위
- tracker, 문서, 코드와 테스트의 현재 근거 및 충돌
- 확정된 contract, ownership, invariant와 주요 tradeoff
- stable ID와 명확한 첫 slice가 있는 완전한 dependency-ordered plan
- 각 slice의 delivered outcome, dependency, relevant seam, constraint 또는
  invariant, observable acceptance criteria와 proportionate verification
- 남은 결정 각각의 해결, 명시적 보류 또는 blocker 상태

Slice는 독립적으로 가이드·위임·리뷰·진행도 보고가 가능하고 완료 후
저장소를 일관된 상태로 남기는 단위다. 계획을 재정렬할 때 기존 ID를
보존하고 split, merge, addition과 removal을 기록한다.

## 구현으로 전환

Design readiness gate가 끝나면 issue 문서의 stable slice plan을 구현 진입점으로 사용한다.

- 사용자가 구현 가이드를 요청하면 `$lean-implementation`의 Guide mode로 다음 slice의 파일, public seam, 핵심 흐름, 불변식과 제외 범위를 설명하고 파일은 수정하지 않는다.
- 사용자는 가이드를 판단해 직접 구현하거나, 일부 또는 전체 typing을 agent에게 위임할 수 있다.
- Agent가 구현을 위임받으면 한 번에 현재 slice 하나만 수정·검토하고 전체 plan 진행도를 같은 stable ID로 표시한다. 일반 구현 요청은 그 slice 뒤에 멈추며 여러 slice 또는 남은 계획 전체를 명시한 요청만 batch를 허용한다.
- 테스트와 실행 범위는 `$lean-implementation`을 따른다.
- Test-first는 독립적인 expected result와 기존 seam 등 suitability gate가 통과한 slice에서만 사용한다.
- 실제 코드와 검증 결과가 slice split, merge, reorder 또는 contract 변경을 요구하면 `$refine-issue`로 돌아가 계획부터 갱신한다.

## 문서 구조

Issue design 문서는 필요한 범위에서 다음 정보를 유지한다.

- Issue 번호, 제목과 URL
- 문서가 다루는 목표와 제외 범위
- stable ID가 있는 dependency-ordered implementation slice와 구현 순서
- 확정된 계약과 관련 ADR
- 각 slice의 outcome, dependency, seam, invariant, acceptance와 verification
- 현재 상태와 다음 미해결 결정

단순한 항목 수를 맞추기 위해 빈 section을 추가하지 않는다. 현재 issue에 필요한 구조만 사용한다.

## 최종 tracker sync

Refinement와 progress reporting은 GitHub Issue를 수정하지 않는다. 사용자가
최종 반영을 명시적으로 요청한 경우에만 delivered outcome, material scope
change, verification evidence와 한계, design/ADR/commit 또는 PR reference,
deferred·remaining·excluded work, 요청한 최종 state나 close operation을
tracker에 반영한다.
