# Issue #5: Vulkan World Lab — 클라이언트 기반과 확장 계획

- Issue: [Vulkan World Lab: 독립 렌더러와 대규모 투사체·AOI·군중 이동](https://github.com/jammer-droid/PrivateServerToolKit/issues/5)
- 선행 결과: [#6 Vulkan 2D 렌더러 기반 학습](issue-6-vulkan-2d-foundation.md), S1-1~S1-6 완료.
- 상태: S2-1 실행 구조 완료. 클라이언트 기반 1/6 완료, 다음은 S2-2 수학·ECS다. 상세 구현 계약은 단계 진입 시 확정한다.
- 진행 방식: 사용자가 직접 구현하고 agent가 개념 안내와 코드·실행 검토를 담당한다. 문서 작성은 구현 위임이 아니다.

## 목표와 범위

World Lab을 첫 사용 사례로 삼아 다른 프로젝트에서도 활용할 수 있는 클라이언트 개발 기반을 만든다. 실행 루프, 씬, 수학, ECS, 입력, 리소스, 렌더링과 계측을 실제 동작을 통해 발전시킨다. 현재 목적은 GPU 계산 백엔드를 관찰·검증하는 클라이언트이며, 이후 렌더링 실험이나 실행 구조 확장을 수용할 수 있게 한다.

에디터를 갖춘 범용 게임 엔진을 먼저 만들지는 않는다. 범용성을 예상해 대규모 프레임워크를 선행 구현하기보다 World Lab에서 확인한 책임과 경계를 재사용 가능하게 만든다. 디버그 UI, 시나리오 설정과 관찰 도구는 포함하지만 범용 씬 에디터·에셋 제작 도구는 필수가 아니다.

작업은 `src/vulkan/`에서 독립적으로 빌드·실행한다. 이슈 #5의 명시적 독립성 계약에 따라 ToolKit의 `TkResult`, 배포용 DLL/C ABI, Service Host, Packet Tool, 루트 빌드 편입을 강제하지 않는다. [공용 결과 타입 ADR](../adr/0003-use-tkresult-for-cpp-failures.md)과 [shared-library ADR](../adr/0006-fix-shared-library-public-boundary-to-c-abi.md)을 변경하는 작업도 아니다. 실제 서버·네트워크 통합, 범용 3D 엔진과 복잡한 에셋 파이프라인은 현재 범위 밖이다.

공개 문서에는 구현 방향과 목표를 기록한다. 특정 강의·개인·레퍼런스 프로젝트 이름을 계획의 계약이나 단계 이름으로 사용하지 않는다. 참고 자료와 세부 구현 방법이 달라져도 목표와 완료 기준은 유지한다.

## 현재 근거와 계획의 수준

- GitHub #5는 OPEN이며 목표·상위 범위·완료 조건의 기준이다. 본 문서는 그 범위 안의 진행 순서와 세부 계약을 소유한다.
- #6의 완료 기록과 기존 `renderer_project`가 출발점이다. 현재 소스는 `src/vulkan/vulkan_app/`과 `src/vulkan/vulkan_runtime/`으로 분리했다. GLFW 창, Vulkan 실행 기반, Renderer2D와 도형 인스턴싱을 유지한다.
- 수학 기반, ECS 월드, 카메라·미니맵, Render Graph, Visual Profiler와 월드 계산 백엔드 비교는 앞으로 구현할 내용이다. 기존 `compute_project`의 실습을 해당 백엔드의 검증 완료로 보지 않는다.
- #5 본문에 보존된 시안은 S0에서 저장소로 옮겨야 한다. 현재 시안의 표시 규모는 실제 처리량 증거가 아니다.
- 루트 `CONTEXT.md`의 서버 측 WorldRuntime과 이 클라이언트 기반을 동일한 모듈로 확정하지 않는다. 공유 실행 계층이나 공용 ABI를 새로 정의하지 않는다.
- 현재 확인한 이슈와 합의한 방향 사이에 범위 충돌은 없다. 단, 공개 인터페이스 등 미정 사항이 있으므로 문서 전체가 곧 구현 준비 완료 상태인 것은 아니다.

## 고정 흐름과 Vulkan 확장의 관계

기본 흐름은 다음 순서로 유지한다.

**실행 구조 → 수학·ECS → 상호작용 → 리소스·표현 → 카메라·여러 화면 → 관측·확장 → World Lab 적용**

기본 흐름은 클라이언트 기능을 쌓는 줄기이고, Vulkan 확장 목록은 각 단계에서 필요한 기술을 선택하는 목록이다. 확장 항목은 ECS Component와 다른 계획 단위다. 기술 목록을 소진하기 위해 순서를 바꾸거나 실제 사용 사례 없이 기능을 추가하지 않는다.

- 단계에 진입하면 agent가 현재 구현과 확장 목록을 검토해 도입할 항목과 이유를 제안한다.
- 선행 조건, 현재 단계에서 해결할 문제, 검증 가능한 최소 범위가 있는 항목을 선택한다.
- 사용자와 해당 단계의 범위를 합의한 뒤 세부 slice에 연결한다. 후보 등록 자체는 구현 권한이 아니다.
- 실제 구현에서 발견한 새 후보는 목록에 추가할 수 있다. 기존 ID는 유지하고 분할·순서 변경·보류 이유를 기록한다.
- Render Graph와 CPU/GPU Visual Profiler는 #5의 필수 결과다. 도입 시점과 구현 방법은 조절하되 선택 후보처럼 영구 제외하지 않는다.
- Compute 백엔드도 후속 필수 결과다. MSAA, bindless 같은 개별 확장 후보 전부를 구현해야 완료되는 계획은 아니다.

단계별 기록에는 `상태 / 선택한 확장 ID / 선택 이유 / 상세 slice / 검증 증거 / 남은 결정`을 유지한다. 상태는 미착수·설계 중·구현 중·검증 대기·완료·보류로 구분하고, 보류에는 재검토 조건을 적는다.

## 상위 단계와 의존 관계

기존 S0~S7 ID와 순서를 보존한다. 아래는 탐색 경로이며 이슈 본문의 시나리오 계약과 상위 완료 조건을 대체하지 않는다.

| ID | 선행 | 제공 결과 | 완료 증거와 현재 상태 |
|---|---|---|---|
| S0 | 없음 | 시안 원문·실행 방법 보존, 조작·표시·범위 결정 | 저장소에서 시안 재현 및 결정 목록. 미완료 |
| S1 | #6에서 S0와 독립 진행 | Vulkan 2D 렌더러 기반 | #6의 구현·실행 기록. 기반 학습 완료 |
| S2 | S1 | 재사용할 클라이언트 구조, 넓은 월드·플레이어·카메라 | S2-1 완료, S2-2~S2-6 미완료. 재현 가능한 이동·월드 경계는 후속 검증 |
| S3 | S2 | 공간 인덱스와 CPU 단일→병렬→Vulkan Compute 백엔드 | 전수 정답과 비교, 빈/밀집/경계 및 overflow 검증. 미착수 |
| S4 | S3 | 관찰자별 AOI와 진입·유지·이탈 표시 | 백엔드별 집합 일치, 셀 크기 변경·생성/제거 검증. 미착수 |
| S5 | S4 | 투사체 이동 경로 기반 피격 판정과 CPU 상태 적용 | 빠른 이동·다중 대상·동률·무효 ID 판정 비교. 미착수 |
| S6 | S5 | 경로장·군중 이동과 혼잡 관찰 | 목적지·장애물·도달 불가·경계와 백엔드 정확성 검증. 미착수 |
| S7 | S6 | 세 시나리오 통합과 화면 없는 반복 측정 | 재현 명령·원본 결과·전체 비용 비교·환경별 한계. 미착수 |

S0의 보존 작업과 S2의 구조 설계는 독립적으로 진행할 수 있다. S2에서 실제 월드 조작과 표시를 확정할 때는 S0의 관련 결정을 반영한다. 아래 기본 흐름은 S2를 구체화하고 마지막 World Lab 적용은 S3~S7로 이어진다. Render Graph와 계측은 S2에서 최소 경로를 만들고 이후 시나리오에서도 확장한다.

## 클라이언트 기반 단계

각 항목은 여러 학습 slice를 포함할 수 있는 안정적인 단계다. 단계 진입 시 첫 slice의 파일·인터페이스·제외 범위와 검증을 확정한다. 완료 여부는 각 단계의 상태와 검증 기록을 따른다.

### S2-1 — 실행 구조

- **상태:** 완료. 사용자 최종 확인으로 최소화·복원·종료 정상 동작을 확인했다.

- **선행/학습:** S1. 애플리케이션 조립, 씬의 수명과 갱신, 종료 책임을 익힌다.
- **결과/seam:** 창·이벤트·실행 루프·렌더러와 World Lab 전용 장면을 연결하는 경계. 기존 `main.cpp`, `app`, `renderer`, `core`를 출발점으로 삼는다.
- **불변식:** 도메인 로직을 Vulkan 초기화에 넣지 않는다. 기존 GPU 자원 재사용과 종료 순서를 보존한다. 클래스 수를 늘리는 것 자체를 목표로 삼지 않는다.
- **완료/검증:** 기존 도형 장면이 새 실행 구조에서 표시되고 resize·최소화/복원·종료가 유지된다. 독립 빌드와 해당 실행 경로를 확인한다.
- **확정:** `vulkan_app`과 `vulkan_runtime`을 별도 디렉터리·CMake 타깃으로 분리한다. runtime을 독립 빌드·설치한 후 앱이 `find_package(VulkanRuntime CONFIG REQUIRED)`와 `VulkanRuntime::Runtime` 타깃으로 소비한다. 최초 `add_subdirectory` 구성은 이 독립 패키지 구성으로 대체했다.
- **후속 공개 경계:** 공유 라이브러리와 C++ 인터페이스를 사용한다. 앱/runtime은 같은 호환 도구 체인으로 함께 빌드하며, C++17 자체가 ABI 호환성을 보장한다고 가정하지 않는다. 앱의 Game은 앱 소유, Application은 참조로 빌리고 렌더링 입력은 호출 동안 읽어 내부 자원으로 복사한다. 공개 경계에서 Vulkan 타입·함수와 구현 저장 구조를 숨긴다. 내부 Vulkan을 다중 API용으로 추상화하지 않는다.
- **현재 작은 작업:** SHARED 전환과 WorldSandbox 연결 이후 Application/Pimpl로 실행 루프를 옮겼다. 앱 소스는 Game·설정·Run 호출만 사용한다. SDK는 공개 헤더 5개만 설치하며 Vulkan/GLFW는 PRIVATE 링크 의존성이다. 내부 클래스 8개의 export를 제거했다. Application 공개 함수와 현재 예외 전달용 VulkanException의 export는 유지한다.
- **셰이더:** 현재 `7daf3cc`의 asset 구성에 따라 runtime이 builtin 소스를 컴파일·설치하고, 앱 asset 타깃이 실행 파일 옆 `shaders/runtime`에 복사한다. 앱의 추가 셰이더는 별도로 컴파일한다. 세부 빌드 동작은 [스크립트 사용법](../../src/vulkan/script/README.md)을 따른다. 앱은 builtin 경로를 `ApplicationConfig::shaderDirectory`로 전달하고 Application이 즉시 복사한다. 최초 앱 컴파일/공유 출력 경로 구성에서 이 방식으로 갱신됐다.
- **빌드:** 아래 독립 빌드·패키지 사용 절차를 따른다. 각 프로젝트의 `.clangd`는 자신의 `build/dev/compile_commands.json`을 참조한다. 앱 빌드는 runtime 소스를 다시 컴파일하지 않는다.
- **SDK include 접두사:** 앱은 `runtime/app/Application.h`, `runtime/app/IGame.h`처럼 설치 헤더를 구분한다. 원본 헤더는 runtime의 기존 app/common/renderer 폴더에 유지하고, 설치 경로만 `include/runtime/`으로 둔다. 설치 타깃은 `${prefix}/include`와 `${prefix}/include/runtime`을 모두 전달한다. 따라서 앱은 `runtime/...`으로 구분하고, 공개 헤더 내부에서는 기존 `common/...`, `renderer/...` 참조를 유지한다. 접두사는 앱의 표기 규칙이며, 두 검색 경로를 제공하므로 접두사 없는 include를 기술적으로 금지하지는 않는다. 생성된 `VulkanRuntimeExport.h`는 기존 SDK include 루트에 유지한다. 이 변경은 기존 dev preset의 runtime build/install과 앱 build로 검증했다.
- **현재 패키지:** Application/IGame/DrawData2D/ClassTraits와 생성된 export 헤더만 설치한다. 패키지 config는 Vulkan/GLFW 개발 패키지를 찾지 않는다. 예외 타입 export와 build-tree include 노출은 남아 있으며, 장기 ABI 안정성을 약속하는 배포 API는 아니다.

### S2-2 — 수학과 ECS

- **선행/학습:** S2-1. 벡터·기하 연산과 Entity 식별·수명, Component 저장, System 갱신을 직접 구현한다.
- **결과/seam:** CPU 수학 타입, EntityManager와 기본 Transform/표현 데이터, 월드 데이터에서 렌더링 입력을 추출하는 경계.
- **불변식:** ECS가 Vulkan 핸들의 소유자가 되지 않는다. CPU 타입과 GPU 전송 layout을 동일하다고 가정하지 않는다. 생성·제거와 순회 중 변경의 적용 시점을 정의한다.
- **완료/검증:** ECS로 도형을 생성·이동·제거한다. 0 길이 벡터, 기본 연산, 빈 월드, 제거 후 ID 접근 및 ID 재사용 정책을 작은 독립 사례로 확인한다.
- **남은 결정:** 직접 작성할 수학 연산과 외부 수학 라이브러리의 범위, ECS 저장 구조와 ID 정책. 특정 ECS 라이브러리로 직접 구현을 대체하지 않는다.

### S2-3 — 상호작용과 시뮬레이션 시간

- **선행/학습:** S2-2. 입력 Action, 이동·기초 충돌, 고정 틱과 입력 기록을 연결한다.
- **결과/seam:** 이벤트→Action→tick 입력→System 갱신→렌더링 스냅샷. 플레이어 조작과 단순 충돌 장면을 제공한다.
- **불변식:** 렌더링 프레임 수가 시뮬레이션 진행량을 결정하지 않는다. 충돌 응답·입력 소비 시점·느린 프레임의 catch-up 정책을 명시한다. S5의 연속 피격 판정 전체를 여기서 구현하지 않는다.
- **완료/검증:** 같은 seed와 tick 입력의 최종 상태를 비교하고 렌더링 주기를 바꿔 확인한다. 입력 해제·경계·겹침과 일시적인 긴 프레임을 검증한다.
- **남은 결정:** 틱 간격, 재생 데이터 형식, 부동소수점 비교 기준과 catch-up 한계.

### S2-4 — 리소스와 표현

- **선행/학습:** S2-3. 텍스처·스프라이트·애니메이션과 리소스 수명을 익힌다.
- **결과/seam:** 리소스 요청/소유, Sprite 데이터, Renderer2D의 텍스처 표현 경계. V-01~V-03 도입을 검토한다.
- **불변식:** 월드 객체 수명과 공유 리소스 수명을 구분한다. GPU 사용 중인 데이터는 덮어쓰거나 파괴하지 않는다.
- **완료/검증:** 서로 다른 텍스처의 도형과 애니메이션을 표시하고 공유 사용·제거·로딩 실패를 확인한다. 변경한 셰이더와 앱을 빌드하고 관련 validation 진단을 확인한다.
- **남은 결정:** 리소스 식별자·캐시와 로더 범위. 범용 에셋 파이프라인은 만들지 않는다.

### S2-5 — 카메라와 여러 화면

- **선행/학습:** S2-4. 월드/카메라/화면 좌표, 추적 카메라와 미니맵, 패스 간 리소스 의존성을 익힌다.
- **결과/seam:** 월드와 뷰의 분리, offscreen 타깃, 합성 및 Render Graph의 최소 실행 경로. V-04 후 V-05를 연결한다.
- **불변식:** AOI 대상 선정과 화면 컬링을 구분한다. 패스의 리소스 읽기·쓰기는 명시한다. 카메라 두 개가 월드 시뮬레이션을 두 번 갱신하지 않는다.
- **완료/검증:** 같은 월드를 추적 화면과 미니맵으로 표시하고 이동·경계·resize를 확인한다. 실제 다중 패스의 순서와 배리어, 잘못된 의존 선언의 진단을 검증한다.
- **남은 결정:** 카메라 좌표 규약, graph의 초기 리소스/패스 모델과 재생성 책임. 최초 버전의 다중 큐·메모리 aliasing은 필수가 아니다.

### S2-6 — 관측과 확장

- **선행/학습:** S2-5. 실행 비용의 구분, 계측 데이터 기록·시각화, 측정에 근거한 저장 구조 개선을 익힌다.
- **결과/seam:** 직접 구현한 CPU 스코프 계측과 Visual Profiler, V-06의 GPU 패스 시간, 실행·리소스 통계. 간단한 CPU 계측은 앞 단계에서 필요할 때 먼저 넣을 수 있다.
- **불변식:** CPU 시간과 GPU 시간을 근거 없이 하나의 타임라인에 정렬하지 않는다. GPU 구간만으로 전체 가속을 주장하지 않는다. 계측 자체의 비용도 구분한다.
- **완료/검증:** System·업로드·기록·대기와 GPU 패스 시간을 구분해 표시·저장한다. 프레임 슬롯 재사용과 결과 지연 회수를 확인한다. 메모리 풀·캐시 개선은 필요성이 측정된 대상에 적용하고 전후를 비교한다.
- **남은 결정:** trace 형식·UI 도구, 초기 통계 항목과 측정 환경. 범용 외부 profiler로 직접 구현 목표를 대체하지 않는다.

## Vulkan 확장 목록

모든 항목은 아직 이 후속 계획에서 미착수다. 기존 실습에서 API를 사용한 경험과 World Lab에 적용·검증한 상태를 구분한다. 아래 연결 단계는 검토 시점이며 자동으로 확정된 추가 작업은 아니다.

| ID / 항목 | 목표 | 선행 조건 / 연결 단계 | 최소 완료 기준 | 범위 |
|---|---|---|---|---|
| V-01 업로드 경로 | Buffer/Image에 데이터를 전송하고 사용 시점을 관리 | 자원 소유 경계 / S2-4 | 작은 텍스처·버퍼 업로드, 전송 후 사용과 임시 자원 수명 검증 | 텍스처 표현에 필요한 범위 |
| V-02 Texture·Sampler·Descriptor | 여러 이미지와 셰이더 입력을 연결 | V-01, 셰이더 입력 계약 / S2-4 | 서로 다른 텍스처 표시, descriptor와 참조 자원 수명 검증 | 텍스처 표현에 필요한 범위 |
| V-03 Indexed Draw·배칭 | 도형 데이터와 draw 구성 방식을 비교 | 리소스·draw 데이터 모델 / S2-4 이후 | 출력 일치와 draw/업로드 비용 비교, 투명 도형 순서 보존 | 선택 후보 |
| V-04 Offscreen 타깃 | 월드·미니맵을 독립 이미지에 그리고 합성 | V-01~02, 뷰 모델 / S2-5 | 출력 이미지 재사용·샘플링·resize와 수명 검증 | 다중 화면 기반 |
| V-05 Render Graph | 읽기·쓰기 의존에서 순서와 상태 전환을 관리 | V-04의 실제 다중 패스 / S2-5 | world/minimap→합성 등의 그래프 실행, 배리어와 잘못된 선언 진단·구조 표시 | 필수 결과 |
| V-06 GPU Query·시각 계측 | GPU 패스 비용을 CPU 구간과 구분 | CPU 계측, 프레임 슬롯 / S2-6 | query 결과를 완료 후 회수해 프레임/패스에 대응, 계측을 위한 즉시 대기 회피 | 필수 결과 |
| V-07 메모리 관리·지연 해제 | 반복 할당과 런타임 자원 교체를 관리 | 자원 사용 완료 추적과 통계 / S2-4~6 이후 | 교체·제거 시 사용 중 자원 보존, 회수 시점과 메모리 사용 검증 | 규모·교체 요구에 따라 선택 |
| V-08 Shader 인터페이스·Pipeline 관리 | 인터페이스 검사와 중복 생성/재생성 관리 | 여러 셰이더·파이프라인 사용 / S2-4 이후 | 잘못된 입력 계약 진단 또는 재사용/교체 검증 | reflection·cache·reload는 각각 선택 |
| V-09 MSAA·Resolve | 도형 가장자리 품질과 비용 비교 | V-04와 sample 수 계약 / S2-5 이후 | resolve 출력·resize 검증, 품질과 비용 비교 | 선택 후보 |
| V-10 Compute·Storage·Readback | 같은 계산 계약의 GPU 백엔드 | S2, CPU 정답과 공간 인덱스 / S3 이후 | 결과·overflow 비교, 업로드~회수·적용까지 비용 측정 | 필수 결과 |
| V-11 GPU 컬링·Indirect Draw | GPU 결과를 렌더링에 연결 | V-10 및 명시적 CPU 기준 / S3 이후 | 표시 결과 일치와 간접 명령 동기화, 이득/손해 측정 | 선택 후보 |
| V-12 Descriptor 확장·Bindless | 다수 리소스 접근과 바인딩 비용 실험 | V-02 및 실제 병목/수량 요구 / 후속 단계 | 단순 경로와 결과·수명·비용 비교 | 선택 후보 |

새 항목은 같은 열을 채워 추가한다. 지원·활성화 누락은 현재 미해결 과제로 등록하지 않는다. 새 기능의 실제 요구 조건은 해당 기능 구현 시 확인한다.

## 공통 계약과 검증 원칙

- 계산 코어는 창과 렌더러 없이도 검증할 수 있게 한다. 월드 입력 스냅샷→백엔드 결과→CPU 검증·상태 확정→표시 순서를 유지한다.
- Entity ID, 생성·제거 시점과 결과 적용 순서를 정의한다. GPU로 ECS 내부 포인터를 넘기지 않는다.
- CPU 단일 스레드 기준 구현 후 동일 알고리즘·입력·계약의 CPU 병렬/GPU 경로를 비교한다. 공간 알고리즘 개선과 GPU 전환 효과를 분리한다.
- 가변 길이 결과의 capacity·overflow·중복·누락 정책을 명시한다. 조용한 잘라내기를 허용하지 않는다.
- 수학·ECS·조회 등 독립적인 정답이 있는 로직은 작은 자동 검증을 사용한다. 표시·resize·자원 수명은 관련 실행과 validation 기록으로 확인한다. 실행하지 않은 검증은 통과로 기록하지 않는다.
- 성능은 초기화, 입력 준비, 제출, GPU 실행, 완료 대기, 회수와 최종 적용을 구분한다. seed·분포·반복·빌드 설정과 환경을 남긴다. macOS/MoltenVK와 native Vulkan의 측정 결과를 구분한다.
- 기반 선택은 직접 구현할 학습 대상을 존중한다. 외부 수학·메모리·UI·계측 도구의 채택 범위는 단계별로 결정하며 목록에 등장했다고 의존성으로 확정하지 않는다.

## 학습 수준과 단계 진입 규칙

- **구현 수준:** CPU 벡터·기하 연산, ECS 수명과 갱신, Action/고정 틱, 리소스 소유, 카메라 변환, 패스 의존/배리어, CPU/GPU 계측, 백엔드 정확성 비교.
- **개념 수준:** 장기적인 모듈 재사용, 여러 큐의 병렬 실행, 데이터 배치와 캐시 비용. 실제 선택한 확장에 필요해지면 구현 수준으로 올린다.
- **현재 생략 가능:** 범용 에디터, 다중 그래픽 API 추상화, PBR/3D 에셋 시스템, 선행 bindless·메모리 aliasing·다중 큐 스케줄러.

새 메커니즘은 개념→최소 실행 예제→사용자의 구현·실행→검토로 진행한다. 익숙한 부분은 목표·제약·완료 증거를 제시하고 사용자가 설계하도록 한다. 구현 순서를 상세 API 호출 목록으로 미리 고정하지 않는다.

단계 시작 전에 해당 범위의 결과, dependency, seam, ownership/invariant, 완료 기준과 검증을 작은 slice로 확정한다. 장기 미정 사항은 관련 단계까지 보류하지만 현재 slice의 동작이나 경계를 바꾸는 결정은 먼저 해결한다.

## 다음 결정과 첫 실험

1. **S2-2 다음:** GLM을 내부 계산에 도입하고 WorldSandbox의 좌표 계산에서 공개 DrawItem2D로 변환하는 경계를 정한다. API에 GLM 타입은 노출하지 않는다. 이후 ECS 식별·저장·갱신 계약을 구체화한다.
2. **S0 및 관련 화면 구현 전:** 시안 보존 위치와 조작·구역 정책을 정리한다. 새로 정해야 하는 범위를 기존 시안과 구분한다.
3. **각 단계에서 결정:** 수학 라이브러리 경계, ECS 저장 구조, 고정 틱, 리소스 ID, graph 모델과 profiler 형식은 위 단계의 미정 목록을 따른다.
4. **S3~S6 상세 설계까지 보류:** 시나리오별 부하·정확성, AOI 정책 전환, 충돌 동률, 군중 경로/회피 모델과 CPU/GPU 역할 분담. 상위 이슈의 필수 시나리오는 유지한다.

첫 실험은 **기존 도형 장면을 최소 Application/씬 경계 뒤에서 실행하고, 동일한 resize·최소화/복원·종료 동작을 확인하는 것**이다. 디렉터리 분리 이후 이 실행 경계를 구현한다. ECS와 Render Graph를 이 첫 실험에 함께 넣지 않는다.

## 독립 빌드와 패키지 사용

runtime 디렉터리에서 먼저 실행한다.

```sh
cmake --preset dev
cmake --build --preset dev
cmake --install build/dev
```

개발 SDK는 `src/vulkan/vulkan_runtime/build/dev/sdk`에 생성된다. 라이브러리, 현재 필요한 헤더, 셰이더 소스와 `lib/cmake/VulkanRuntime`의 config/targets 파일을 포함한다. 시스템 설치나 관리자 권한은 필요하지 않는다. runtime 소스·헤더·셰이더를 수정하면 빌드·설치를 갱신한다.

그다음 앱 디렉터리에서 실행한다.

```sh
cmake --preset dev
cmake --build --preset dev
./build/dev/vulkan_app
```

앱 preset의 `CMAKE_PREFIX_PATH`는 개발 SDK를 가리킨다. 다른 소비자는 자신의 preset이나 configure 인자로 SDK prefix를 제공하고 `VulkanRuntime::Runtime`을 링크한다. 패키지의 include/link 설정은 소비자에게 전달되지만 runtime의 빌드 preset을 실행하거나 병합하지 않는다. 독립 runtime의 Debug/Release와 소비자의 도구 체인·구성을 호환되게 유지한다.

패키지는 설치 위치를 기준으로 경로를 계산한다. SDK를 다른 디렉터리로 복사해도 원래 runtime 소스/빌드 경로를 요구하지 않는다. 현재 SDK 소비에는 Vulkan/GLFW 개발 패키지가 필요하지 않다. runtime 빌드에는 이 개발 의존성이 필요하고, 앱은 사용자 셰이더를 위한 glslc 실행 파일과 Python만 별도로 찾는다. 다만 실행 환경에는 링크한 Vulkan Loader와 GLFW 공유 라이브러리가 존재해야 한다. 이 SDK가 third-party 라이브러리를 함께 배포하는 것은 아니다.

## S2-1 공유 라이브러리 계약

- `vulkan_runtime`은 SHARED이며 `VulkanRuntime::Runtime` 패키지 이름은 유지한다. macOS의 dylib, Windows의 DLL/import library, Linux의 shared object는 CMake가 플랫폼에 맞게 생성한다. 실제 검증 환경은 macOS/AppleClang 16/libc++ Debug이며 다른 플랫폼은 미검증이다.
- `GenerateExportHeader`가 생성한 `VulkanRuntimeExport.h`를 SDK에 설치한다. 기본 visibility는 hidden, 외부에서 호출하는 out-of-line 함수는 `VULKAN_RUNTIME_API`로 공개한다. 기존 `VulkanException`은 typed catch의 RTTI/vtable을 위해 타입도 공개한다.
- runtime 소유 자원을 정리하는 기본 소멸자를 `.cpp`로 옮겼다. 기존 클래스 layout, STL 인자/반환, Vulkan 타입과 inline getter는 임시 C++ 경계에 남아 있다. 같은 compiler/standard library/호환 runtime 설정으로 빌드하며 헤더 변경 시 앱도 재빌드한다. 이 단계는 ABI 안정화 완료가 아니다.
- 기존 예외 정책은 이 전환에서 보존한다. runtime의 `VulkanException`/표준 예외를 앱에서 잡는 임시 계약이며 예외 비활성 빌드나 서로 다른 C++ 런타임 조합은 지원 계약이 아니다. Application의 최종 오류/콜백 정책은 다음에 확정한다.
- Unix의 개발 앱은 imported shared target으로부터 CMake가 생성한 build RPATH로 SDK 라이브러리를 찾는다. Windows는 post-build에서 runtime DLL을 실행 파일 옆에 복사한다. Third-party DLL 배치와 설치된 앱의 배포 경로는 이 작업의 검증 범위 밖이다.
- 기존 dev SDK에 과거 `.a`가 남아 있어도 갱신된 imported target은 공유 라이브러리를 가리킨다. 새 SDK 설치 경로에서는 공유 라이브러리만 설치된다.

## S2-1 공유 라이브러리 검증

- macOS에서 runtime configure/build/install 및 앱 configure/build 통과. `otool -L`로 앱의 `@rpath/libvulkan_runtime.dylib` 참조를 확인했다.
- 새 임시 SDK에는 dylib만 설치되고 `.a`가 없음을 확인했다. 그 SDK를 사용하는 별도 소비자를 빌드했으며 runtime 소스 빌드와 GLFW package 탐색 없이 링크했다.
- `nm`으로 공개 함수·소멸자와 예외 RTTI가 존재하고 `ReadSpirV`, 내부 Instance 생성 helper, Window 내부 callback 설정 함수가 외부 심볼에 없음을 확인했다. 표준 라이브러리에서 발생하는 일부 심볼은 남아 있으며 엄격한 export allowlist는 아직 제공하지 않는다.
- 임시 boundary consumer에서 runtime이 던진 `VulkanException`의 typed catch·메시지·결과 코드, 성공 경로, 두 차례 Instance 생성·소멸을 확인하고 exit 0으로 종료했다.
- 원본 앱의 임시 복사본에만 4프레임 후 종료 조건을 넣어 공유 라이브러리 기반 렌더링·정상 종료(exit 0)를 확인했다. 제품 코드에 자동 종료 옵션은 추가하지 않았다. resize·최소화는 재검증하지 않았다.
- 실행 로그에 과거 `DemoteToHelperInvocation` capability 관련 `VUID-VkShaderModuleCreateInfo-pCode-08740` 진단이 다시 관찰됐다. 이번 작업은 셰이더/feature 동작을 변경하지 않았으며 이 진단의 수정은 포함하지 않는다. 따라서 validation 무오류 실행으로 기록하지 않는다.
- 임시 검증 자료: `/var/folders/32/hd2fw_xd7yg_hsc4n6lj201w0000gn/T/vulkan-shared-check-0d7knufn/`의 소비자 소스·빌드·로그. 임시 자료는 영구 테스트 자산이 아니며 검증 범위와 결과는 이 문서에 보존한다.
- Windows/Linux 실행, third-party 라이브러리를 포함한 독립 배포, 최종 Pimpl/Game 공개 API는 미검증/후속 범위다.

## S2-1 독립 패키지 검증 이력

- runtime의 `dev` configure/build/install 성공. 자체 compile database와 `build/dev/sdk` 생성 확인.
- 앱의 `dev` configure/build 성공. runtime 소스를 빌드하지 않고 설치된 라이브러리를 링크하며, runtime의 `build/dev/shaders`에 셰이더 생성 확인.
- SDK를 임시 위치로 복사하고 앱의 `main.cpp`/CMake만 별도 consumer 디렉터리에 복사해 새 빌드 성공. 해당 compile database는 `main.cpp` 한 개이며 설치된 include 경로를 사용하고 원래 runtime 소스/빌드 경로를 참조하지 않음을 확인했다.
- 패키지 생성 파일의 원래 runtime 경로 유출 여부와 `git diff --check`, 신규 CMake/preset 공백 검사를 확인했다. GUI 실행은 이번 작업에서 수행하지 않았다.

## S2-1 최초 디렉터리 분리 검증 이력

- 앱 디렉터리에서 새 `dev` 빌드 트리로 configure/build 성공. `vulkan_app`, `vulkan_runtime/libvulkan_runtime.a`, `vulkan_runtime/shaders/*.spv` 생성 확인.
- 이동한 C++·셰이더 파일 26개의 내용이 기존 커밋과 동일함을 확인했다. CMake와 preset, 편집기 compile database 경로만 조정했다.
- `git diff --check`와 새 빌드 설정의 공백 검사를 통과했다. 디렉터리 이동 이후 GUI 실행·resize 확인은 아직 하지 않았다.
- 기존 `renderer_project/build`는 과거 빌드 캐시로 남겨두었으며 새 빌드에서 사용하지 않는다. S2-1 전체는 Application/공개 인터페이스 작업이 남아 있다.

## S2-1 Application 실행 경계와 검증

- `ApplicationConfig`는 `const char* shaderDirectory`만 공개한다. null/빈 문자열은 생성자가 거부하고, 유효한 문자열은 Impl의 `std::filesystem::path`로 복사한다. 원본 문자열은 생성자 이후 유지하지 않아도 된다. 파일 존재와 SPIR-V 로딩 실패는 Run에서 기존 예외 경로로 전달한다.
- 앱은 WorldSandbox를 소유하고 Application은 `IGame&`를 빌린다. Game은 Application보다 오래 살아야 한다. Impl은 runtime에서 생성·파괴하며 공개 헤더에는 포인터만 둔다.
- Window·Context·Surface·Swapchain·FrameResources·Renderer2D와 명령 기록/제출은 `Application.cpp` 안에서 처리한다. 이 단계에서는 Run의 지역 자원으로 기존 역순 파괴를 유지한다. 정상 종료와 Game 예외 모두 GPU 대기를 자원 파괴 전에 수행한다.
- `Run()`은 메인 스레드에서 동기 실행하고 Game 콜백도 같은 스레드에서 순차 호출한다. 동시·재진입 호출은 지원하지 않는다. 고정 틱은 아직 도입하지 않았으며 기존 fence 대기 이후 Update→GetDrawData→복사/업로드 순서를 유지한다.
- 최소화 대기 중 종료 요청을 받은 경우 재생성을 진행하지 않고 루프 종료로 이어지게 했다. 기존 미사용 `prevFormat` 변수도 제거했다.
- runtime 빌드·설치와 앱 빌드 통과. 앱 main/WorldSandbox와 공개 Application/IGame 헤더에서 Vulkan 타입·함수·core 헤더 직접 사용이 없음을 확인했다. SDK의 광범위한 헤더 설치와 Vulkan PUBLIC 의존성은 아직 유지한다.
- 설치된 SDK를 사용하는 임시 consumer에서 null/빈 설정 거부, 잘못된 셰이더 경로 실패, 5회째 Game 갱신 예외 전달을 검증했다. 이어 정상 경로에서 75회 Game 갱신, 두 번의 창 크기 변경, 창 닫기 후 Run 반환을 확인했다. 생성자에 전달한 경로 문자열을 지운 뒤에도 실행됐고, Application 파괴 후에도 Game 데이터가 유효했다.
- 임시 consumer의 창 조작은 검증용 Cocoa 코드이며 제품 코드에 추가하지 않았다. 검증 자료는 `/var/folders/32/hd2fw_xd7yg_hsc4n6lj201w0000gn/T/application-api-check-nwav2lwt/`에 있다. 결과 exit 0, VUID 진단 없음. 최소화·복원과 다른 OS 실행은 이번 검증에서 제외했다.
- `5fa0dc2`에서 shaderDemoteToHelperInvocation 지원 검사·활성화 후 기존 Demote 진단이 해소됐다. 아래 오류 기록은 수정 전 이력이다.

## S2-1 공개 SDK 정리 검증

- 기존 `dev` preset을 그대로 사용해 사용자에 의해 비워진 두 프로젝트의 `build/dev`에서 새로 configure/build했다. runtime은 `cmake --install build/dev`로 기존 preset의 SDK 위치에 설치했다. 별도 preset·SDK 검증 경로를 추가하지 않았다.
- 설치 헤더는 `VulkanRuntimeExport.h`, `app/Application.h`, `app/IGame.h`, `renderer/DrawData2D.h`, `common/ClassTraits.h`의 5개다. `core`, Window, Renderer2D 헤더는 SDK에 없다.
- Vulkan과 GLFW는 runtime의 PRIVATE 링크 의존성이다. 패키지 config의 Vulkan 탐색을 제거했고 앱은 `find_program(GLSLC_EXECUTABLE)`로 셰이더 컴파일 도구만 찾는다. 설치된 imported target에 Vulkan/GLFW 링크 요구사항이 없고 앱의 컴파일 명령이 SDK include만 사용하는지 확인했다.
- 첫 실행에서 Vulkan Loader의 `@rpath/libvulkan.1.dylib`를 찾지 못했다. runtime의 Unix INSTALL_RPATH에 발견한 Vulkan/GLFW 라이브러리 디렉터리를 명시하고 다시 빌드·설치해 해결했다. 이 경로는 현재 개발 환경의 외부 라이브러리 위치이며 third-party 재배포/다른 장비 지원 증거는 아니다.
- 같은 `dev` 앱 실행에서 로딩 성공과 VUID 없는 실행 로그를 확인했다. GUI 자동화 도구가 이 독립 실행 파일을 식별하지 못해 창 조작·정상 닫기는 이번에 재검증하지 않았고, 검증 프로세스는 SIGTERM으로 정리했다. 기존 정상 종료 검증과 구분한다. 로그는 앱의 `build/dev/sdk-check.log`에 있다.
- preset 변경 없음, `git diff --check` 통과. 내부 심볼 export는 이번 범위에서 유지했다.

## S2-1 내부 export 정리 검증

- Window, VulkanContext, Swapchain, FrameResources, HostVisibleBuffer, ShaderModule, GraphicsPipeline, Renderer2D 헤더에서 export 매크로와 불필요한 export 헤더 include를 제거했다. 구현과 소멸자 위치는 유지했다.
- Application 생성자·소멸자·Run 및 현재 예외 전달 계약에 필요한 VulkanException 타입 export는 유지했다. IGame과 데이터 타입에는 export를 추가하지 않았다.
- 기존 dev preset의 runtime build/install 및 앱 build 통과. `nm -gU` 결과에서 위 내부 클래스의 함수가 사라지고 Application 함수와 예외 RTTI가 남아 있음을 확인했다. 표준 라이브러리의 부수 심볼까지 제거하는 엄격한 allowlist는 이번 범위가 아니다.
- 설치된 공개 헤더와 runtime 라이브러리만 사용한 소비자에서 잘못된 셰이더 경로의 std::exception catch, 정상 도형 데이터 처리 후 6회째 Game 갱신 예외 전달과 자원 정리를 확인했다. 검사 프로그램 exit 0, VUID 오류 없음. 정상 창 닫기·resize는 이번에 재검증하지 않았다.
- 검증 소스·로그·심볼 목록은 앱의 기존 `build/dev/export-boundary-check.cpp`, `export-boundary-check.log`, `export-symbols.txt`에 있다. dev preset은 변경하지 않았다. `git diff --check` 통과.

## S2-1 최종 완료

- 실행 앱과 재사용 runtime을 독립 CMake 프로젝트 및 shared 라이브러리 패키지로 분리했다. Application/Pimpl이 Vulkan 실행을 소유하고, 앱 소유 WorldSandbox를 IGame 참조로 호출한다.
- 공개 도형 뷰를 runtime 내부 GPU 데이터로 변환하고, SDK 헤더·링크 의존성·내부 export를 정리했다. 기존 dev preset으로 빌드·설치와 소비자 검증을 수행했다.
- shaderDemoteToHelperInvocation 지원 검사·활성화 이후 관련 validation 오류가 사라졌으며 사용자도 확인했다.
- 자동 검증의 렌더링·resize·오류 경로·자원 정리 증거에 더해, 사용자가 최소화·복원·종료의 정상 작동을 최종 확인했다. S2-1을 완료 처리한다.
- Windows/Linux 실행·재배포와 장기 ABI 호환성은 검증 완료로 간주하지 않는다. 예외 타입 export는 현재 계약으로 유지하며 S2-1 완료를 막는 미구현 항목으로 두지 않는다.
- 다음 단계는 S2-2다. S0와 S2-2~S2-6 및 S3~S7은 완료되지 않았다.

## 변경 기록

- SDK 헤더 설치 경로에 `runtime/` 접두사를 추가했다. 앱의 include를 변경하고 runtime 소스 배치는 유지했다. 두 INSTALL_INTERFACE 경로가 설치된 타깃에 전달되는 것을 확인했으며, 기존 dev preset의 runtime 빌드·설치와 앱 빌드가 통과했다.

- S2-1 후속: 패키지 분리 커밋 `94244c9` 이후 SHARED·명시적 export·runtime 측 소멸자·소비자 로딩 경로를 추가했다.

- S2-1 후속: runtime 자체 preset과 install/export 패키지를 추가하고 앱의 `add_subdirectory`를 `find_package`로 교체했다. 셰이더 소스도 패키지에 포함하며 앱에서 컴파일한다.

- S2-1 후속: 앱/runtime 디렉터리와 CMake를 분리했다. C++ 공유 라이브러리 공개 경계는 다음 작업으로 기록했다.

- 2026-09-18: 최초 작성. 기존 S0~S7을 보존하고 S2-1~S2-6의 기본 흐름과 V-01~V-12 확장 목록을 추가했다. 확장 선택 시점과 필수/선택 범위를 분리했다. 소스 코드·GitHub 상태 변경은 포함하지 않는다.
