# Domain documents

이 저장소는 단일 컨텍스트 구성을 사용한다.

- 루트 도메인 문서: `CONTEXT.md`
- 루트 ADR 디렉터리: `docs/adr/`
- 컨텍스트별 도메인 문서 및 ADR: 현재 없음
- `CONTEXT-MAP.md`: 사용하지 않음

GitHub Issue 단위의 구현 계획과 상세 design은 `docs/design/issue-<number>-<slug>.md`에 기록한다. Issue design 문서는 domain glossary나 ADR을 대체하지 않는다. 여러 issue에 적용되는 용어는 `CONTEXT.md`, 되돌리기 어려운 구조적 결정은 `docs/adr/`에 별도로 남기고 issue design 문서에서 연결한다.

작업 전에 존재하는 `CONTEXT.md`와 관련 ADR을 읽는다. 도메인 문서가 아직 없으면 별도의 경고나 문서 생성을 요구하지 않고 진행한다.

도메인 문서가 있으면 기존 용어집의 용어를 그대로 사용한다. 요청이나 구현이 기존 ADR과 충돌하면 ADR을 조용히 무시하거나 덮어쓰지 말고 충돌 내용을 사용자에게 알린다.
