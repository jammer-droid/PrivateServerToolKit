# GitHub Issue design workflow

## 문서 단위와 경로

구현 계획과 grilling을 통해 확정한 상세 design의 단위는 GitHub Issue다. 문서는 다음 경로에 issue당 하나로 유지한다.

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
4. 구현 결과를 바꾸는 물질적 결정을 grilling에서 한 번에 하나씩 확정한다.
5. 확정된 결정을 해당 issue 문서의 관련 Phase/slice section에 반영하고, 미해결 항목은 결정된 것처럼 작성하지 않는다.
6. 공유 용어나 cross-issue 구조 결정이 생기면 `CONTEXT.md` 또는 ADR을 갱신하고 issue 문서에서 연결한다.
7. 문서 링크, `git diff --check`, issue 범위와 완료 기준을 검증한다.

Issue 생성, 본문·라벨·관계 변경, comment와 close는 publish다. 로컬 design 문서 작성이 허용되어도 GitHub 원격 상태를 자동으로 변경하지 않는다.

## 구현으로 전환

Design grilling이 끝나면 issue 문서의 현재 Phase와 다음 slice를 구현 진입점으로 사용한다.

- 사용자가 구현 가이드를 요청하면 `$lean-implementation`의 Guide mode로 다음 slice의 파일, public seam, 핵심 흐름, 불변식과 제외 범위를 설명하고 파일은 수정하지 않는다.
- 사용자는 가이드를 판단해 직접 구현하거나, 일부 또는 전체 typing을 agent에게 위임할 수 있다.
- Agent가 구현을 위임받으면 같은 slice만 수정하고 issue 문서의 이후 slice를 앞당겨 구현하지 않는다.
- 테스트와 실행 범위는 `$lean-implementation`을 따른다.
- 실제 코드와 검증 결과가 design 가정을 깨뜨릴 때만 해당 issue 문서로 돌아가 계약을 갱신한다.

## 문서 구조

Issue design 문서는 필요한 범위에서 다음 정보를 유지한다.

- Issue 번호, 제목과 URL
- 문서가 다루는 목표와 제외 범위
- 현재 Phase/slice와 구현 순서
- 확정된 계약과 관련 ADR
- 완료 및 검증 기준
- 현재 상태와 다음 미해결 결정

단순한 항목 수를 맞추기 위해 빈 section을 추가하지 않는다. 현재 issue에 필요한 구조만 사용한다.
