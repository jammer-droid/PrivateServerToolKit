# Issue tracker

## 기준 저장소

- 트래커: GitHub Issues
- 저장소: `jammer-droid/PrivateServerToolKit`
- URL: <https://github.com/jammer-droid/PrivateServerToolKit/issues>
- 외부 PR은 트리아지 탐색 대상에 포함하지 않는다. PR 작성자가 외부 기여자인지와 관계없이 PR에서 별도 이슈로 승격된 항목만 이슈 트리아지 흐름에 넣는다.

저장소 문맥에서 bare reference `#123`은 `jammer-droid/PrivateServerToolKit`의 123번 항목으로 해석한다. 번호가 PR을 가리키면 외부 PR 정책을 적용하고, 다른 저장소 항목은 `owner/repository#123` 또는 전체 URL로 적는다.

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
