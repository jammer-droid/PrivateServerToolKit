# Issue tracker

## 기준 저장소

- 트래커: GitHub Issues
- 저장소: `jammer-droid/PrivateServerToolKit`
- URL: <https://github.com/jammer-droid/PrivateServerToolKit/issues>
- 외부 PR은 트리아지 탐색 대상에 포함하지 않는다. PR 작성자가 외부 기여자인지와 관계없이 PR에서 별도 이슈로 승격된 항목만 이슈 트리아지 흐름에 넣는다.

저장소 문맥에서 bare reference `#123`은 `jammer-droid/PrivateServerToolKit`의 123번 항목으로 해석한다. 번호가 PR을 가리키면 외부 PR 정책을 적용하고, 다른 저장소 항목은 `owner/repository#123` 또는 전체 URL로 적는다.

## Issue design 문서

GitHub Issue의 구현 계획과 세부 design은 `docs/design/issue-<number>-<slug>.md`에 issue당 하나로 관리한다. Issue의 Phase와 하위 slice는 독립 계획 문서로 분리하지 않고 해당 issue design 문서 안에 section으로 둔다.

- GitHub Issue는 목표, 범위, 상태와 상위 완료 조건의 source of truth다.
- Local issue design 문서는 세부 계약, 구현 순서, 하위 Phase, 검증 기준과 확정된 결정을 소유한다.
- 문서는 issue URL을 포함하고 작성 전에 `gh issue view <number> --repo jammer-droid/PrivateServerToolKit`으로 현재 issue를 확인한다.
- Issue가 지정되지 않은 상태에서 issue를 임의로 생성하거나 별도 구현 계획 문서를 만들지 않는다.

작성과 갱신 절차는 [`design-workflow.md`](design-workflow.md)를 따른다.

## 이슈 작업

GitHub CLI 사용 시 저장소를 명시한다.

```sh
gh issue list --repo jammer-droid/PrivateServerToolKit
gh issue view 123 --repo jammer-droid/PrivateServerToolKit
gh issue create --repo jammer-droid/PrivateServerToolKit --title "제목" --body "내용"
gh issue comment 123 --repo jammer-droid/PrivateServerToolKit --body "댓글"
gh issue edit 123 --repo jammer-droid/PrivateServerToolKit --add-label "needs-triage"
gh issue edit 123 --repo jammer-droid/PrivateServerToolKit --remove-label "needs-triage"
gh issue close 123 --repo jammer-droid/PrivateServerToolKit --reason completed
gh issue close 123 --repo jammer-droid/PrivateServerToolKit --reason "not planned"
```

생성, 댓글, 라벨 변경, 종료처럼 GitHub 원격 상태를 바꾸는 작업을 **publish**로 본다. 로컬 초안 작성과 읽기 전용 조회는 publish가 아니다. Publish는 사용자에게 허용된 범위에서만 수행한다.

## 차단 관계

본문의 체크리스트나 텍스트 표기 대신 GitHub의 네이티브 issue dependency를 사용한다.

1. GitHub에서 대상 이슈를 연다.
2. 오른쪽 사이드바의 **Relationships**를 선택한다.
3. 대상 이슈가 다른 이슈를 기다리면 **Mark as blocked by**, 다른 이슈를 막고 있으면 **Mark as blocking**을 선택한다.
4. 연결할 이슈를 찾아 선택한다.

연결 후 GitHub 이슈의 **Relationships**에서 `Blocked by`와 `Blocking` 관계를 확인한다.

## Wayfinding operations

Wayfinder map은 `wayfinder:map` 라벨이 있는 GitHub Issue 하나로 표현한다.
하위 decision ticket에는 역할에 따라 다음 라벨을 사용한다.

- `wayfinder:research`
- `wayfinder:prototype`
- `wayfinder:grilling`
- `wayfinder:task`

명시적 Wayfinder 요청 전에 현재 라벨을 조회한다. canonical 라벨이 없으면
다른 라벨로 대체하거나 자동 생성하지 말고, 사용자가 라벨 생성을 별도로
허용할 때까지 멈춘다.

- 저장소에서 native sub-issue를 사용할 수 있으면 map의 child로 연결한다.
  사용할 수 없으면 map 본문의 linked checklist와 child 본문의
  `Part of #<map>` 표기를 함께 사용한다.
- blocking edge는 위의 native **Relationships**를 우선한다. 사용할 수 없는
  경우에만 child 본문의 `Blocked by: #<n>` 필드를 fallback으로 사용한다.
- claim은 child assignee로 표현하며 assignee가 없으면 unclaimed다.
- live frontier는 map의 open child 중 open blocker와 assignee가 없는 항목을
  map 순서대로 조회한 결과다.
- resolution은 child에 결론을 남기고 닫은 뒤 map의 one-line decision index를
  갱신한다. 이 외부 write들은 범위를 명시한 Wayfinder chart 또는 advance
  요청이 있을 때만 수행한다.
