# Issue #6: Vulkan 2D 렌더러 기반 구축과 단계별 학습

- Issue: <https://github.com/jammer-droid/PrivateServerToolKit/issues/6>
- Parent: [#5 Vulkan World Lab](https://github.com/jammer-droid/PrivateServerToolKit/issues/5)의 S1
- 상태: 구현 완료 1/6. S1-1 complete, S1-2 current, S1-3~S1-6 pending.
- 진행 모드: `study-guide` Study session + `lean-implementation` Guide. 사용자가 구현하며 agent는 안내·검토한다. 구현 수정은 별도 요청 범위에서만 한다.

## 목표와 경계

Instance부터 Graphics Pipeline 기반 2D 렌더러까지 직접 구성하면서 Vulkan 객체의 역할, ownership, lifetime과 동기화를 학습·복습한다. 향후 GPU 계산 백엔드에 재사용할 이해와 기반을 얻되 이번 실행 경로에는 Compute Pipeline을 사용하지 않는다.

작업 루트는 `src/vulkan/`이며 독립 CMake 실행 경로를 유지한다. 부모 이슈의 독립성 계약에 따라 ToolKit 제품의 TkResult, DLL/C ABI, Service Host, Packet Tool, 루트 빌드 편입을 강제하지 않는다. 공통 native 기반을 제품 라이브러리로 만드는 작업은 아니다.

포함: Instance/Device/Queue, 창·Surface·Swapchain, Graphics 명령 제출, Vertex/Fragment Shader, 2D 도형과 Instancing, resize·최소화·종료 수명.

제외: Compute Pipeline, 기존 Compute 실습의 사전 리팩터링, GPU 계산 백엔드, AOI·충돌·군중 계산, 실제 플레이어 이동·추적 카메라·미니맵, 성능 벤치마크, 범용 엔진과 에셋 파이프라인. 표시용 궤적은 준비된 데이터로 검증한다.

## 현재 근거와 상위 계획 조정

- 최초 검토한 `src/vulkan/main.cpp`는 780줄의 Compute 실습이다. Instance/Device, Buffer/Memory, Descriptor, Compute Pipeline, Command Buffer/Fence와 순차 작업 검증이 한 함수에 있다. 이를 참고 자료로 보존하며 Graphics 구현의 완료 증거로 간주하지 않는다.
- 최초 검토한 `src/vulkan/CMakeLists.txt`는 C++17 및 Vulkan을 사용하는 독립 실행 타깃이다. 창·Surface·Swapchain·Graphics Pipeline은 아직 없다.
- 기존 코드는 Vulkan 1.3을 요청하고 MoltenVK를 선택하며 Compute Queue를 찾는다. 새 경로는 Surface에 대한 Present 지원과 Graphics 지원을 확인해야 한다.
- 기존 이슈 #5는 S0 → S1 순서였으나 사용자는 이번 기반 학습을 우선하기로 했다. 이 서브 이슈는 S0 완료를 선행 조건으로 두지 않는다. S0 보존 작업은 미완료로 남으며 부모의 완료 조건을 축소하지 않는다.
- 부모의 S1 ID는 유지한다. 아래 S1-1~S1-6은 본 서브 이슈가 소유하는 하위 단계다. 부모 S2~S7은 이 문서에서 재정의하지 않는다.
- 문서 작성 시 코드 실행 검증은 수행하지 않았다. 기존 실행에 대한 부모 이슈 기록과 이번 실행 증거는 구분한다.

문서 작성 중 worktree에서 기존 파일들이 `src/vulkan/compute_project/` 아래로 이동한 상태를 확인했다. 이 이동은 이번 문서 작업의 변경이 아니며 보존한다. 다음 안내에서는 해당 경로의 Compute 실습을 참고하고 Graphics 실행 골격은 별도로 구성한다. 이동 후 빌드·실행은 아직 검증하지 않았다.

## 학습 방식과 개념 깊이

한 단계 안에서도 작은 실행 가능한 단위로 진행한다. 개념 설명 → 정확한 코드·자료 위치 확인 → 작은 예시 또는 흐름 설명 → 사용자의 구현·실행 → 검토와 짧은 이해 확인 순서다. 익숙해진 개념은 목표 중심 과제로 전환하고, 새 핵심 기법은 먼저 함께 학습한다. 질문만으로 진도를 막거나 완성된 프레임워크 코드를 먼저 제공하지 않는다.

- **구현 수준:** 객체 소유·파괴 순서, extension/feature 협상, Graphics/Present Queue 선택, Acquire–Submit–Present, Command Buffer 재사용, Fence/Semaphore, 이미지 layout, Shader 입출력, 2D 좌표와 instance 데이터.
- **개념 수준:** 메모리 유형·배치 전략, Render Pass와 Dynamic Rendering의 차이, CPU/GPU 병렬 진행과 프레임 수/Swapchain 이미지 수의 차이.
- **보류:** Shader Reflection, Pipeline Factory, 범용 Pool, Bindless, Render Graph, 복잡한 allocator, PBR·모델 로딩과 대규모 최적화.

진행 기록에는 구현 결과와 이해 증거를 분리한다. 실행하지 않은 결과는 통과로 적지 않고 사용자가 생략한 학습은 숙달 대신 skipped로 기록한다.

## 구조와 공통 계약

다음은 책임의 목표이며 선행 생성할 클래스 목록이 아니다. 실제 파일은 각 단계에서 필요한 만큼 추가한다.

### 디렉터리 구성 — 확정

`src/vulkan/renderer_project/`의 C++ 구성은 `app`, `renderer`, `core`, `common`으로 나눈다. Vulkan 실행 기반 디렉터리 이름은 `core`로 사용한다.

```text
renderer_project/
├── CMakeLists.txt
├── main.cpp
├── app/
│   ├── Application.h/.cpp
│   └── Window.h/.cpp
├── renderer/
│   ├── Renderer2D.h/.cpp
│   └── DrawData2D.h
├── core/
│   ├── VulkanContext.h/.cpp
│   ├── Swapchain.h/.cpp
│   └── FrameResources.h/.cpp
├── common/
│   ├── VulkanHandle.h
│   └── VulkanError.h/.cpp
└── shaders/
```

위 파일명은 책임별 배치 기준이며 기존 파일의 이름을 변경하거나 미구현 파일을 미리 생성하라는 의미는 아니다. `main.cpp`와 빌드 설정은 프로젝트 루트에, Shader 소스는 `shaders/`에 둔다.

- `app`: 창·이벤트·실행 루프와 구성 요소 조립, 전체 종료 순서를 담당한다.
- `renderer`: 2D 표시 데이터와 Graphics Pipeline·draw 명령 구성을 담당한다. `DrawData2D`는 가능하면 Vulkan 핸들 없이 위치·크기·색상 등으로 표현한다.
- `core`: Instance·Device·Queue, Swapchain과 프레임 자원 등 Vulkan 실행 기반을 담당한다. `VulkanContext`는 `core/VulkanContext.h/.cpp`에 배치한다.
- `common`: 여러 구성 요소가 사용하는 오류 전달·핸들 소유권 도구를 둔다. 앱 설정이나 렌더링 정책은 소유하지 않는다.
- 의존 방향은 `app → renderer → core → common`을 기본으로 하고 `app → core` 직접 사용도 허용한다. 하위 계층이 상위 계층을 참조하지 않으며, 필요한 계층은 `common`을 직접 사용할 수 있다.
- Swapchain은 출력 이미지와 재생성을, Renderer2D는 표시할 도형과 명령 기록을 담당한다. Context에 모든 자원을 모으지 않고 각 구성 요소가 자기 자원을 소유한다. 부모 자원의 수명과 GPU 완료 대기는 상위 구성에서 보장한다.

### 소유권과 수명

| 책임 | 소유할 내용과 경계 |
|---|---|
| 실행 골격/App | 창 이벤트, 실행 루프, 조립과 종료 순서 |
| Vulkan 초기화 | Instance, debug messenger, Physical Device 선택, Device와 Queue 정보 |
| 창/출력 | 창과 Surface, Swapchain 및 소유 Image View, 크기 변경 상태 |
| 프레임 자원 | Command Pool/Buffer, CPU 재사용을 제어할 Fence와 acquire 동기화, 프레임 입력 버퍼 |
| Renderer2D | Graphics Pipeline·도형 자원, 표시 데이터를 소비하는 draw 기록 |

- Instance가 Surface와 Device보다 오래 살고 Device가 자신의 자원보다 오래 산다. Queue와 Physical Device는 별도 destroy하는 소유 자원이 아니다. Swapchain image도 앱이 직접 생성·파괴하거나 메모리를 해제하지 않는다.
- GPU 사용이 끝나기 전에 자원을 파괴하거나 CPU가 덮어쓰지 않는다. 초기화 중간 실패도 이미 획득한 자원을 정확히 한 번 정리한다.
- 창 생성 전에 필요한 instance extension을 조회하고, Surface 생성 후 해당 Surface에 대한 Present 지원을 확인하여 Device/Queue 선택을 완료한다. Headless 경로를 깨뜨리는 창 의존성을 공통 초기화에 무조건 넣지 않는다. Headless 구현 자체는 이번 필수 결과가 아니다.
- API 버전 요청과 feature 활성화는 구분한다. 실제 지원을 확인하고 필요한 feature를 명시적으로 켠다. 미지원 환경에서 조용히 진행하지 않는다.
- 프레임 슬롯 번호와 Swapchain image index는 서로 다르다. Present 완료를 submission Fence만으로 추정하지 않는다. Present 대기용 semaphore 재사용은 이미지별 관리 등 공식적으로 보장되는 방식으로 설계한다.
- 화면 크기는 framebuffer pixel 크기로 다룬다. 0 크기에는 렌더링을 중지하고 복원 이벤트를 처리한다. Out-of-date와 suboptimal 결과의 제어 흐름을 명시한다.
- Renderer2D는 표시 데이터를 읽으며 월드 상태, AOI나 충돌 판정을 소유하지 않는다.
- 기존 Compute 실습은 보존한다. Graphics 실행 경로는 `src/vulkan/renderer_project/`, 타깃은 `vulkan_renderer`로 분리했다.

## 단계별 계약

### S1-1 — 실행 골격과 Instance

- **선행:** 없음. 2026-09-13 완료. 첫 실험은 Instance 생성·정상 파괴였다.
- **결과/seam:** `src/vulkan/renderer_project/CMakeLists.txt`, `main.cpp`, `core/VulkanContext.h/.cpp`. 버전·extension·layer 조회, Instance와 debug messenger 생성·정리.
- **학습:** Loader와 Instance의 역할, 지원 버전과 요청 버전, 필수/선택 extension, validation과 오류 반환, RAII 수명.
- **불변식:** 실제 지원을 확인한다. 성공과 실패 경로 모두 소유 자원을 정리한다. Device나 Compute Pipeline은 만들지 않는다.
- **완료 증거:** 독립 빌드·실행 명령, 선택 버전·설정 로그, 정상 종료, 요청한 필수 기능이 없을 때 설명 가능한 실패 경로. Debug validation 설정을 확인하고 오류를 해결한다.
- **이해 확인:** Instance가 실제 GPU 선택과 어떻게 다른지, debug messenger가 왜 Instance보다 먼저 정리되는지 설명한다.

### S1-2 — 창·Surface와 Device/Queue

- **선행:** S1-1.
- **결과/seam:** 창 관리와 Vulkan 초기화 경계. 창 시스템이 요구하는 instance extension을 반영하고 Surface를 만든 뒤 GPU·Graphics/Present Queue와 Device를 구성한다.
- **학습:** Physical/Logical Device 차이, queue family와 queue, Surface별 Present capability, device extension/feature.
- **불변식:** Graphics와 Present가 같은 family라는 가정을 하지 않는다. 첫 지원 정책은 구현 전에 정하고 분리 family 미지원 시 명확히 거부하거나 올바르게 지원한다. Device 선택을 MoltenVK driver ID에 고정하지 않는다.
- **완료 증거:** 선택 GPU·queue family·지원 기능 로그, 창 열기/닫기, 안전한 정리와 validation 확인. Swapchain 출력은 아직 요구하지 않는다.
- **이해 확인:** Graphics 지원만으로 화면 출력 가능성을 판단할 수 없는 이유를 설명한다.

### S1-3 — Swapchain과 Image View

- **선행:** S1-2.
- **결과/seam:** 출력 자원 묶음. Surface capability에 맞는 format, extent, image count, present mode 선택과 Swapchain/Image View 생성·정리.
- **학습:** 이미지와 view, 창 크기와 framebuffer 크기, 앱 소유 자원과 borrowed image.
- **불변식:** 지원 범위 내 설정, 각 image에 대응하는 view, Surface·Device·Swapchain의 수명 준수. 고정 RGBA format이나 image count를 가정하지 않는다.
- **완료 증거:** 선택 설정과 실제 image count 로그, 생성/정리 validation 확인. 0 크기 처리와 다음 단계 재생성 진입 조건 정리. 실제 resize 출력 검증은 S1-4에서 한다.
- **이해 확인:** Swapchain image와 앱이 만든 Image View의 정리 책임 차이를 설명한다.

### S1-4 — 프레임 루프와 배경색

- **선행:** S1-3.
- **결과/seam:** 프레임 자원, 명령 기록·제출, Acquire–Submit–Present 루프와 Swapchain 재생성 경로.
- **학습:** Fence/Semaphore, 명령 재사용, 이미지 layout 및 접근 동기화, 출력 attachment clear. Compute Pipeline은 사용하지 않는다.
- **불변식:** 사용 중인 자원 재사용 금지, acquire 실패로 제출하지 않는 경로에서 Fence 대기를 영구히 막지 않음, 안전한 present semaphore 재사용, GPU 사용 완료 후 교체 자원 정리.
- **완료 증거:** 배경색 출력, 연속 resize·최소화·복원·종료, 0 크기에서 무의미한 제출 방지, out-of-date 처리와 validation 확인. 반복 횟수·환경은 실제 수행한 만큼 기록한다.
- **이해 확인:** image index와 frame slot의 차이, CPU가 프레임 자원을 다시 쓸 수 있는 시점을 설명한다.

### S1-5 — Graphics Pipeline과 2D 도형

- **선행:** S1-4.
- **결과/seam:** Vertex/Fragment Shader 빌드, pipeline layout·Graphics Pipeline, draw 명령과 2D 입력 데이터. 삼각형에서 위치·크기·색상을 입력받는 도형으로 확장한다.
- **학습:** vertex 처리·rasterization·fragment 출력, viewport/scissor, attachment format, 좌표 변환과 blending.
- **불변식:** Shader와 CPU 데이터 layout 일치, pipeline/attachment format 호환, 좌표계와 크기 단위 명시. Shader 실행 경로가 우연한 작업 디렉터리에 의존하지 않도록 빌드·실행 계약을 정한다.
- **완료 증거:** 재현 가능한 Shader 빌드, 삼각형·사각형 표시, 위치·크기·색상 변경 결과, resize 후 표시와 validation 확인. 실제 2D 좌표 변환은 독립적으로 계산 가능한 예시 값으로 확인한다.
- **이해 확인:** 정점의 좌표가 화면 pixel로 바뀌는 과정을 설명한다.

### S1-6 — Instancing과 최소 장면

- **선행:** S1-5.
- **결과/seam:** instance 데이터 업로드·draw, Renderer2D 책임 정리, 도형·선·준비된 궤적을 조합한 탑다운 장면.
- **학습:** per-vertex/per-instance 데이터, 버퍼 capacity와 갱신 수명, 도형 조합, 표시 순서와 투명도.
- **불변식:** 0개 처리, capacity 초과의 명시적 정책과 누락 방지, GPU 사용 중 CPU 덮어쓰기 방지. 시뮬레이션과 성능 주장은 포함하지 않는다.
- **완료 증거:** 0개·1개·다수·capacity 경계, 서로 다른 위치·크기·색상, 선·궤적, 창 수명 회귀 확인과 validation 결과. 최종 장면 캡처 및 실행 명령.
- **이해 확인:** Instancing이 어떤 데이터를 공유하고 무엇을 개별 입력으로 받는지, 프레임 입력 수명을 설명한다.

## 단계 진입 시 확정할 결정

아래는 확정된 선택이 아니다. 앞 단계와 무관한 선택으로 학습을 막지 않고 해당 단계의 안내에서 하나씩 해결한다.

| 시점 | 남은 결정과 제안 방향 |
|---|---|
| S1-1 | `renderer_project/`의 `vulkan_renderer`, C++17·C Vulkan API, `VK_CHECK`와 예외 기반 초기화 실패 전달을 채택했다. macOS에서 instance 지원 버전 1.4.335와 실행을 확인했고 앱 목표는 1.3이다. 1.3 미만 지원 검사와 macOS portability extension 조회·활성화를 구현했다. Debug의 Validation Layer 조회·활성화와 Release의 비요청 경로를 구현했다. Debug messenger 생성·RAII 정리와 테스트 메시지 수신까지 구현했다. Instance 생성·파괴 진단용 pNext 연결까지 구현했다. |
| S1-2 | 창 라이브러리(GLFW 후보), Graphics/Present 분리 family 지원 정책과 활성화 feature. |
| S1-3~4 | Present mode, frame slot 수, Dynamic Rendering 또는 Render Pass. 기존 Vulkan 1.3 요청을 참고하되 지원 확인 없이 기능 사용을 확정하지 않는다. |
| S1-5 | Shader 언어·컴파일러, 좌표 원점·축·단위, blending과 그리기 순서. |
| S1-6 | 원·선·궤적 표현, instance 데이터와 capacity 정책. |

디버그 UI는 필수 선행 조건이 아니다. 필요성이 생길 때 선택하며 창 이벤트만으로 초기 학습을 시작한다. 추가 플랫폼 검증과 성능 계측은 자동으로 확대하지 않는다.

## 참고 자료

1. [Khronos Tutorial](https://docs.vulkan.org/tutorial/latest/00_Introduction.html) — 개념/실습. 초기화에서 Graphics 출력까지의 흐름을 참고한다. 확인한 최신판은 Vulkan 1.4·C++20·Vulkan-Hpp·Slang 기준이므로 현재 코드에 그대로 복사하지 않는다.
2. [Khronos Instancing](https://docs.vulkan.org/samples/latest/samples/api/instancing/README.html) — S1-6 실습. 같은 mesh에 개별 instance 입력을 주는 방식을 참고한다.
3. [Khronos Swapchain Semaphore Reuse](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html) — S1-4 정확성 reference. submission 완료와 presentation semaphore 재사용 조건을 구분한다.

## S1-1 확정 결정

- 현재 Graphics 실행 코드는 `src/vulkan/renderer_project/`에 둔다. 기존 `compute_project/`와 독립적으로 빌드한다.
- C API 호출을 유지한다. `VK_CHECK`는 `VK_SUCCESS`만 기대하는 호출의 실패를 `VulkanException`으로 전달하고 `main`의 단일 catch에서 진단·종료한다. 재시도·복구가 필요한 결과는 향후 호출 위치에서 별도 처리한다.
- `VulkanHandle`은 단독 소유자로 복사와 이동을 모두 금지한다. 이는 현재 의도적인 제한이다. `Get()`은 소유권을 이전하지 않는 borrowed handle이며 owner 수명 내에서 사용하고 외부에서 파괴하지 않는다.
- Debug messenger는 Instance 뒤에 선언하여 먼저 정리한다. 생성 전에 destroy 함수 주소를 확보·검사하고 생성 성공 직후 빈 래퍼에 `Adopt`한다. 래퍼는 빈 상태를 값 초기화하고 `Adopt`는 비어 있는 소유자만 허용하며 deleter의 nothrow 이동 대입을 요구한다. 래퍼 자체의 복사·이동 금지는 유지한다.
- deleter 구조체를 현재 사용하지만 템플릿이 람다를 금지하는 것은 아니다. 핸들 래퍼 자체의 이동·복사를 허용할 필요가 생기면 해당 단계에서 계약을 재검토한다.
- 후속 구조 논의에서 `VulkanContext`의 책임과 `core/VulkanContext.h/.cpp` 배치를 확정했다. Instance 초기화 정책과 소유권부터 분리하고 이후 Device·Queue로 확장한다. 현재 Instance 생성 함수와 RAII 멤버를 `core/VulkanContext`에 구현했다. Context와 핸들 래퍼 모두 복사·이동 금지를 유지한다.

- Debug 설정 생성 함수 `MakeDebugMessengerCreateInfo`를 공유하며 Debug의 Instance 생성 pNext와 지속 messenger 생성에 동일한 callback·필터를 사용한다. Release의 pNext는 null이다. 연결하는 지역 설정은 vkCreateInstance 호출까지 유효하고 callback은 일반 함수, pUserData는 null로 유지한다.

## 검증과 진행 기록

프레임워크 초기화·출력은 실제 실행과 validation을 주된 증거로 사용한다. 단순 래퍼별 mock test를 추가하지 않는다. 좌표 변환 등 독립 expected result가 있는 로직은 필요할 때 focused test로 검증한다. 장시간 stress·성능 benchmark는 포함하지 않는다.

| 단계 | 상태 | 구현/실행 증거 | 이해 확인 |
|---|---|---|---|
| S1-1 | complete | Debug/Release 빌드·실행 및 Debug callback 수신 통과. 요구 버전·필수 확장·필수 Layer 미지원 진단과 종료 코드 1 확인. 상세 증거와 한계는 아래 기록. | 지원/목표 API 버전 구분, 소유권과 역순 파괴, 생성자 예외 시 멤버 정리, pNext callback과 지속 messenger의 역할 구분 확인. |
| S1-2 | current | GLFW Window와 Surface 연결 후 Debug/Release configure·build 통과. 확장 중복 제거·생성자 초기화 통합·Surface 소유권을 코드 검토했다. Agent의 실제 Surface 생성·GUI 조작 검증은 미수행. GPU·Queue Family 조회를 구현했고 사용자 제공 로그에서 Surface Present 조회 성공을 확인했다. Device 생성은 아직 미구현. | GLFW_NO_API와 Surface를 통한 Vulkan 출력 관계 설명 확인. |
| S1-3 | pending | 아직 없음 | 아직 없음 |
| S1-4 | pending | 아직 없음 | 아직 없음 |
| S1-5 | pending | 아직 없음 | 아직 없음 |
| S1-6 | pending | 아직 없음 | 아직 없음 |

S1-1은 완료했다. 현재 S1-2는 GLFW Window 구현을 마쳤고, 다음 행동은 Device Extension·feature 확인 후 GPU·Graphics/Present Queue 선택이다. Graphics/Present Queue 정책은 GPU 선택 단계에서 확정한다. 후속 진행도·학습 기록은 별도 단계 문서 없이 이 문서의 stable-ID section을 갱신한다.


### S1-1 완료 검증 — 2026-09-13

- macOS/AppleClang 16, instance 지원 버전 1.4.335, 앱 목표 1.3.0. Debug/Release configure·build·실행 모두 종료 코드 0.
- 확장 19개 조회와 macOS portability 활성화, Debug의 Validation Layer 활성화를 확인했다. Debug에서 `Debug messenger callback connected.` 수신, Release stderr는 비어 있었다.
- 원본을 변경하지 않은 임시 프로젝트 복사본에서 요구 버전을 1.99.0으로 높이거나 존재하지 않는 확장·Layer를 요청했다. 세 경우 모두 해당 실패 진단과 종료 코드 1을 확인했다. 임시 변경은 저장소에 포함하지 않는다.
- 빈 목록, `VK_INCOMPLETE` 재조회와 최종 개수 반영은 코드 검토로 확인했다. 이 열거 경계 자체의 실행 주입은 하지 않았다.
- pNext 설정 수명, destroy 함수 선확보, 생성 직후 Adopt, messenger → Instance 정리 순서를 검토했다. Instance 생성·파괴 중 실제 진단 이벤트를 별도로 유발하지 않았고, 모든 부분 초기화 실패를 주입한 것은 아니다. 다른 플랫폼은 실행 검증하지 않았다.
- 사용자 결정에 따라 `SubmitDebugTestMessage` 함수와 Debug 생성 경로의 호출을 유지한다. 이 Warning은 의도한 전달 경로 확인용 메시지이며 실제 API 규칙 위반이나 GPU 작업 결과가 아니다. 제거를 S1-1 완료 조건으로 두지 않는다.

독립 재현 명령(저장소 루트에서 실행):

```sh
cmake -S src/vulkan/renderer_project -B src/vulkan/renderer_project/build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build src/vulkan/renderer_project/build/debug
./src/vulkan/renderer_project/build/debug/vulkan_renderer

cmake -S src/vulkan/renderer_project -B src/vulkan/renderer_project/build/release -DCMAKE_BUILD_TYPE=Release
cmake --build src/vulkan/renderer_project/build/release
./src/vulkan/renderer_project/build/release/vulkan_renderer
```


### S1-2 Window 중간 기록

- 창 라이브러리는 GLFW를 사용한다. `app/Window`는 단일 창과 GLFW 초기화·종료를 소유하고 복사·이동을 금지한다. 현재 다중 창은 범위 밖이다.
- 메인 스레드에서 오류 callback 등록, 초기화, `GLFW_NO_API` 창 생성, 이벤트 대기와 종료를 수행한다. 창 생성 실패 시 GLFW를 종료한 뒤 예외를 전달한다.
- `main`은 Window → VulkanContext 순서로 생성하고 닫기 요청까지 이벤트를 기다린다. 후속 구현에서 Surface를 생성해 지역 RAII 래퍼로 소유한다. 아직 렌더링하지 않는다.
- 클래스 복사·이동 금지 매크로를 `common/ClassTraits.h`로 분리했다. Debug/Release 빌드 통과를 확인했으며 실제 GUI resize·닫기 동작은 agent가 검증하지 않았다.


### S1-2 Surface 중간 기록

- `Window::GetRequiredInstanceExtensions`가 반환한 GLFW 요구 확장을 Context에 전달한다. 외부 요구·portability·debug utils 이름은 문자열 비교로 중복 없이 병합한 뒤 지원 여부를 검사한다.
- 기본 Context 생성자는 확장 목록 생성자로 위임한다. 두 경로 모두 기존 Debug messenger 초기화를 수행한다.
- `Window::CreateSurface`가 생성한 Surface는 `main`의 지역 RAII 래퍼가 즉시 소유한다. 생성 순서는 Window → Context → Surface, 파괴 순서는 Surface → Context → Window다.
- Debug/Release configure·build 통과. 이 기록은 실제 GUI 실행·Surface 생성·창 닫기 검증 완료를 뜻하지 않는다. 해당 실행 증거는 다음 검토에서 확인한다.


### S1-2 GPU 조회 중간 기록

- Physical Device 열거의 0개 처리, VK_INCOMPLETE 재시도, 오류 검사 후 반환 개수 반영을 구현했다. Queue Family 속성과 Surface별 Present 지원을 조회한다. 최종 수정 후 Debug 빌드 통과. 실패 분기의 실행 주입은 하지 않았다.
- 사용자 제공 실행 로그: Apple M4 Pro 후보 2개, API 1.3.323 및 1.3.335, device type 1. 첫 후보는 family 4개, 두 번째는 1개이며 모두 queueCount 1, Graphics/Present true였다. 같은 이름의 후보가 둘인 원인은 아직 조사하지 않았다. 이를 물리 GPU 2개 또는 성능 차이의 증거로 해석하지 않는다.
- 다음 단계에서 Device Extension·feature 조건과 Queue 선택 정책을 정한다. 현재는 후보 조회 결과만 있으며 Logical Device/Queue를 생성한 상태가 아니다.


### S1-2 Device 기능 조회 중간 기록

- GPU별 Device Extension을 조회하여 swapchain 및 portability_subset 노출 여부를 확인한다. portability_subset은 OS 분기 없이 조회하며, 노출하는 GPU를 선택할 때 활성화해야 하는 조건으로 이해한다.
- API 1.3 미만 후보는 이유를 출력하고 다음 후보로 진행한다. Vulkan 1.3 후보에 Features2 → Vulkan13Features 조회 체인을 연결해 dynamicRendering·synchronization2 지원을 출력한다. 조회 결과는 기능 활성화가 아니다.
- Device Extension 열거의 빈 목록·VK_INCOMPLETE·실제 반환 개수 처리와 후보 제외 분기를 코드 검토했다. Debug/Release configure·build 통과. 이번 기능 출력의 실제 GUI 실행과 실패 분기 주입은 수행하지 않았다.
- 다음 단계는 필요한 feature·extension과 Graphics/Present Queue 조건에 따른 최종 후보 선택이다. Logical Device 생성과 feature 활성화는 아직 수행하지 않는다.
