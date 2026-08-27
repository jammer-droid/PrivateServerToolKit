# Triage labels

트리아지 상태에는 아래의 정확한 GitHub 라벨 이름을 사용한다.

| 역할 | GitHub 라벨 |
| --- | --- |
| 신규 분류 대기 | `needs-triage` |
| 추가 정보 필요 | `needs-info` |
| 에이전트가 수행 가능 | `ready-for-agent` |
| 사람의 판단 또는 작업 필요 | `ready-for-human` |
| 처리하지 않음 | `wontfix` |

`ready-for-agent`는 triage가 다음 named agent workflow를 추천했다는 뜻이다.
명시적으로 요청된 작업의 보편적인 prerequisite가 아니며 source 구현을
그 자체로 허가하지 않는다. 구현에는 사용자의 명시적 변경 요청과
`docs/agents/design-workflow.md`의 readiness gate가 모두 필요하다.

이슈 종류를 함께 표시할 때는 기존 분류 라벨을 사용한다.

| 종류 | GitHub 라벨 |
| --- | --- |
| 결함 | `bug` |
| 개선 또는 기능 요청 | `enhancement` |

매핑된 라벨이 GitHub에 없으면 다른 라벨로 임의 대체하지 말고 저장소 설정 충돌로 보고한다.
