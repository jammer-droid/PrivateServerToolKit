# Issue #6: Vulkan 2D 렌더러 기반 구축과 단계별 학습

- Issue: <https://github.com/jammer-droid/PrivateServerToolKit/issues/6>
- Parent: [#5 Vulkan World Lab](https://github.com/jammer-droid/PrivateServerToolKit/issues/5)의 S1
- 상태: 구현 완료 6/6. S1-1~S1-6 complete. Vulkan 2D 렌더러 기반 학습 완료(합의한 보류 사항은 최종 기록 참조).
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

- **상태:** complete. 코드 검토·빌드와 사용자 실행 확인을 바탕으로 완료.
- **선행:** S1-1.
- **결과/seam:** 창 관리와 Vulkan 초기화 경계. 창 시스템이 요구하는 instance extension을 반영하고 Surface를 만든 뒤 GPU·Graphics/Present Queue와 Device를 구성한다.
- **학습:** Physical/Logical Device 차이, queue family와 queue, Surface별 Present capability, device extension/feature.
- **불변식:** Graphics와 Present가 같은 family라는 가정을 하지 않는다. 같은 family를 우선하고 없으면 각 역할의 family를 선택한다. Device 생성 시 서로 다른 family마다 Queue 하나를 요청한다. Device 선택을 MoltenVK driver ID에 고정하지 않는다.
- **완료 증거:** 선택 GPU·queue family·지원 기능 로그, 창 열기/닫기, 안전한 정리와 validation 확인. Swapchain 출력은 아직 요구하지 않는다.
- **이해 확인:** Graphics 지원만으로 화면 출력 가능성을 판단할 수 없는 이유를 설명한다.

### S1-3 — Swapchain과 Image View

- **상태:** complete. 코드·합성 실패 경로 검증과 사용자 실행 확인으로 완료.
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

- **상태:** complete. 코드 검토·빌드, 사용자 도형·블렌딩 화면 및 resize 확인을 바탕으로 완료. 검증 범위는 완료 기록 참조.

- **선행:** S1-4.
- **결과/seam:** Vertex/Fragment Shader 빌드, pipeline layout·Graphics Pipeline, draw 명령과 2D 입력 데이터. 삼각형에서 위치·크기·색상을 입력받는 도형으로 확장한다.
- **학습:** vertex 처리·rasterization·fragment 출력, viewport/scissor, attachment format, 좌표 변환과 blending.
- **불변식:** Shader와 CPU 데이터 layout 일치, pipeline/attachment format 호환, 좌표계와 크기 단위 명시. Shader 실행 경로가 우연한 작업 디렉터리에 의존하지 않도록 빌드·실행 계약을 정한다.
- **완료 증거:** 재현 가능한 Shader 빌드, 삼각형·사각형 표시, 위치·크기·색상 변경 결과, resize 후 표시와 validation 확인. 실제 2D 좌표 변환은 독립적으로 계산 가능한 예시 값으로 확인한다.
- **이해 확인:** 정점의 좌표가 화면 pixel로 바뀌는 과정을 설명한다.

### S1-6 — Instancing과 최소 장면

- **상태:** complete. 사용자 실행 확인, Agent 경계 실행 검사 및 합의한 보류 범위로 완료.

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
| S1-2 | GLFW 채택. API 1.3·swapchain·dynamicRendering·synchronization2 및 Graphics/Present 지원을 요구한다. GPU별 공동 family 우선, 없으면 분리 family를 선택한다. 열거 순서상 첫 적합 GPU를 반환한다. Device 생성 시 family 요청 중복을 제거하고 swapchain·조건부 portability_subset, dynamicRendering·synchronization2를 활성화한다. |
| S1-3~4 | Present mode와 frame slot 수는 단계 진입 시 정한다. Graphics 경로는 Dynamic Rendering·Synchronization2를 사용하며 S1-2에서 지원을 확인하고 Device 생성 시 필요한 feature만 활성화한다. |
| S1-5 | GLSL 450·glslc의 Vulkan 1.3 대상 SPIR-V 빌드. framebuffer 왼쪽 위 원점·오른쪽 +X·아래쪽 +Y·픽셀 단위. 48바이트 Vertex Push Constant로 위치·크기·색상 전달. Straight alpha source-over blending과 draw 순서에 따른 합성. |
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
| S1-2 | complete | Agent의 Debug/Release 빌드 통과, Queue 선택 8개 합성 사례 통과. 사용자 GPU/Surface 조회 로그 및 Device 구현 후 실행 확인. 상세 범위와 한계는 완료 기록 참조. | GLFW_NO_API·Surface·Instance 확장 관계, family index와 queue index, feature 조회/활성화 구분, Queue의 Device 종속 수명 확인. |
| S1-3 | complete | 설정 선택 경계 검사, Debug/Release 빌드, 세 번째 View 생성 실패 시 정리와 정상 정리 순서 검사 통과. 사용자 빌드·실행 확인. | 요청 최소/실제 이미지 개수, borrowed Image와 owned View, 생성자 실패 시 멤버 RAII 정리 이해 확인. |
| S1-4 | complete | 두 프레임 슬롯의 Acquire–Submit–Present, Dynamic Rendering clear와 Swapchain 재생성 구현. 최종 Debug/Release 빌드 통과. 사용자 배경색·validation·종료 및 resize 실행 확인. 정리 관행과 검증 범위는 아래 완료 기록 참조. | frame slot/image index, Fence/Semaphore 재사용, 이미지 subresource와 renderArea, Present 자원 정리의 보장 범위 이해 확인. |
| S1-5 | complete | Shader 빌드·Module·Graphics Pipeline·삼각형/사각형 draw·Push Constant·blending 구현. Agent Debug/Release 빌드, 사용자 화면과 resize 확인. 상세 검증 범위는 완료 기록 참조. | 로컬→픽셀→NDC→viewport 변환, dynamic state, Pipeline format 계약과 attachment·subresource 관계 설명 확인. |
| S1-6 | complete | FIF별 Instance Buffer·Renderer2D·사각형/원/선/궤적 구현. 사용자 장면·창 수명 실행 확인, Agent 0·1·256·257개 및 3→0 전환 실행 검사 통과. 알려진 validation 메시지는 명시적 보류. | instance-rate 입력, 슬롯 Fence 이후 쓰기, 픽셀 크기와 viewport, 궤적 점→선분 변환 이해 확인. |

S1-1~S1-6을 완료하여 이번 Vulkan 2D 렌더러 기반 학습을 마무리했다. Vulkan Client 및 Compute Pipeline·GPU 백엔드는 후속 작업이다. 후속 진행도·학습 기록은 별도 단계 문서 없이 이 문서의 stable-ID section을 갱신한다.


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


다음 중간 기록은 각 작업 당시의 상태이며 최종 상태는 문서 하단의 S1-2 완료 기록을 따른다.

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


### S1-2 GPU·Queue 선택 구현

- `SelectPhysicalDevice(surface)`는 API 1.3, swapchain, dynamicRendering, synchronization2, 유효 Graphics/Present Family를 모두 만족하는 첫 GPU를 반환한다. API 조회 실패는 예외로 전달하며 조건 미충족 후보는 이유를 출력하고 건너뛴다. 전부 탈락하면 명시적으로 실패한다.
- GPU 내부에서는 공동 Graphics/Present Family를 우선하고 없으면 각 역할의 첫 유효 Family를 선택한다. queueCount 0은 제외한다. 분리 family의 실제 Device 생성·동기화 지원은 후속 작업에서 구현한다.
- 사용자 작성 `PhysicalDeviceSelection` 구조체를 유지하고 physicalDevice·두 family index·requiresPortabilitySubset을 반환한다. 핸들과 index는 Instance 수명에 종속된 조회 결과이며 소유권을 이전하지 않는다.
- 기존 `InspectPhysicalDevice`의 전체 목록 출력은 후보별 제외 이유와 main의 최종 선택 GPU·family·portability 출력으로 교체했다. 논리 Device와 Queue는 아직 생성하지 않는다. Surface format·Present mode 등 구체 출력 조건은 S1-3에서 검증한다.
- Debug/Release 빌드 통과. 실제 선택 helper를 임시 검사 프로그램에서 호출해 빈 목록, 역할 누락, queueCount 0, 뒤쪽 공동 family 우선, 분리 family, 첫 공동 family 선택 등 8개 사례를 확인했다. 실제 GPU 선택·GUI 실행은 이번 변경 후 검증하지 않았다.


### S1-2 완료 기록

- `app/Window`가 단일 GLFW 창·이벤트·종료를 관리하고, 필수 Instance Extension을 Context에 전달한다. Surface는 main의 RAII 소유자가 관리한다.
- GPU 선택은 API 1.3 이상, swapchain, dynamicRendering, synchronization2, 사용 가능한 Graphics/Present Family를 요구한다. 같은 family 우선, 없으면 분리 선택하며 열거 순서상 첫 적합 후보를 사용한다.
- `InitializeDevice(selection)`은 같은 Context에서 얻은 유효한 선택 결과로 한 번만 호출하는 계약이다. 중복 초기화를 거부하고, 서로 다른 family마다 queueCount 1·priority 1.0으로 요청한다. 같은 family는 요청 하나만 만든다.
- Device Extension은 swapchain과 선택 GPU가 노출하는 경우의 portability_subset을 요청한다. 새 Vulkan13Features 구조체에서 dynamicRendering·synchronization2만 활성화한다. 조회에 사용한 전체 feature 결과를 그대로 활성화하지 않는다.
- vkCreateDevice 성공 직후 RAII 래퍼가 소유하며, 각 선택 family의 Queue 0을 조회한다. Queue 핸들은 Device가 소유하므로 별도 파괴하지 않는다. Context 멤버는 Device → messenger → Instance 순서로 정리된다.
- Agent 검증: Device 구현의 Debug/Release configure·build 통과, 생성 설정·포인터 수명·성공 직후 소유권 인수·역순 파괴 코드 검토. Queue 선택 helper의 공동/분리 family·역할 누락·빈 목록 등 8개 합성 사례 통과.
- 사용자 검증: 앞서 제공한 Apple M4 Pro 후보의 Graphics/Present 조회 로그와 Device 구현 후 실행 확인 응답을 증거로 기록한다. Agent가 직접 GUI를 조작하거나 Device 실행 로그를 수집한 것은 아니다. 사용자 실행의 빌드 구성별 상세 로그는 별도 제공되지 않았다.
- 한계: 분리 Graphics/Present Family의 실제 하드웨어 실행, 다른 OS, 모든 Device 초기화 실패 분기의 주입은 미수행. 현재는 GPU 작업을 제출하지 않으므로 제출 후 완료 대기·동기화 검증은 S1-4 범위다. 향후 GPU 사용 중 자원을 파괴하지 않도록 종료 흐름을 확장한다.
- 다음 단계 S1-3에서 Surface format·Present mode·extent·image count·usage 조건을 확인하고 Swapchain과 Image View를 생성한다. S1-2 완료가 실제 화면 렌더링이나 모든 출력 조건 검증 완료를 의미하지 않는다.


### S1-3 Surface 지원 조회 중간 기록

- `core/Swapchain.h/.cpp`의 `QuerySwapchainSupport`에서 capabilities·format/color space 쌍·present modes를 조회한다. 모든 VkResult를 검사하고 목록 조회는 빈 결과·VK_INCOMPLETE·실제 반환 개수를 처리한다.
- Window에서 GLFW framebuffer pixel 크기를 조회한다. main에서 지원 범위와 실제 framebuffer 크기를 출력하고 SDK의 `vk_enum_string_helper.h`로 format·color space·present mode 이름을 표시한다.
- Debug/Release 빌드를 확인했고 최종 capabilities 오류 검사 수정 후 Debug 빌드를 확인했다. 사용자 출력에서 HDR format 쌍과 FIFO/Immediate를 확인했으나 이를 선택한 것은 아니다. Agent의 실제 GUI 실행 검증은 미수행.
- 다음 작은 작업은 지원 목록에서 SDR format 쌍·present mode·extent·요청 이미지 개수를 선택하는 것이다. Swapchain과 Image View의 생성·정리는 이후 진행한다.


### S1-3 설정 선택 중간 기록

- `ConfigureSwapchainSettings`는 지원 목록 순서상 BGRA8/RGBA8 SRGB 중 하나와 SRGB_NONLINEAR color space가 같은 항목에서 일치하는 첫 조합을 선택한다. 사용자 결정으로 두 format 간 우선순위는 두지 않는다. Present mode는 FIFO, image usage는 COLOR_ATTACHMENT다.
- 고정 currentExtent는 그대로 사용하고 자유 extent는 framebuffer를 min/max에 clamp한다. framebuffer·고정 extent 및 최종 clamp 결과의 0 크기를 검사한다. 실패 시 output은 갱신하지 않는다.
- 요청 최소 이미지 수는 minImageCount + 1이며 maxImageCount가 0이 아닐 때만 제한한다. 사용자 결정으로 극단적 uint32 이미지 개수 overflow 대응은 현재 실습 범위에서 생략한다.
- 사용자 결정으로 초기 설정이 만들어지지 않으면 main에서 예외로 종료한다. 복원 대기는 이번 시작 경로에 추가하지 않고 resize/최소화 재생성 정책은 S1-4에서 재검토한다. null output은 현재 false를 반환한다.
- 실제 설정 함수의 합성 입력 검사에서 format/color space 쌍, extent clamp, 0 결과 거부·output 보존, 상한 미지정/유한 이미지 수 처리를 확인했다. 최종 Debug 빌드 통과. 현재 장비의 선택 설정 출력은 이번 변경 후 agent가 실행 검증하지 않았다.
- 다음 작업은 Swapchain 생성·RAII 소유와 실제 이미지 목록 조회이며 이후 Image View 생성·정리를 추가한다.


### S1-3 완료 기록

- Swapchain은 선택한 설정·Surface capability·Queue Family 관계로 생성하고 즉시 RAII 소유권을 인수한다. 실제 image 목록은 vkGetSwapchainImagesKHR로 조회하며 개별 Image를 파괴하지 않는다.
- 이미지마다 2D color Image View를 생성한다. format은 Swapchain과 같고 identity swizzle, mip 0 하나·array layer 0 하나를 사용한다.
- 이동 금지 ImageViewHandle은 deque에 빈 소유자를 먼저 emplace하고 vkCreateImageView 성공 직후 Adopt한다. View 컨테이너를 Swapchain 소유자보다 뒤에 선언하여 먼저 파괴한다.
- Agent 검증: Debug/Release 빌드 통과. Vulkan 호출을 대체한 임시 검사에서 세 번째 View 생성 실패 시 앞서 만든 두 View가 Swapchain보다 먼저 정확히 정리되는 것을 확인했고, 정상 생성 시 View 세 개가 Swapchain보다 먼저 정리되는 것도 확인했다. 이는 실제 GPU의 메모리 부족 상황 검증이 아니다.
- 사용자 검증: 최종 코드의 빌드·실행 확인 응답을 받았다. Agent는 이번 최종 GUI 실행을 직접 수행하지 않았다. 실제 이미지 수·View 생성 결과의 상세 실행 로그는 별도 첨부되지 않았다.
- 초기 설정 실패 시 종료, format 우선순위 없음, 극단적 이미지 개수 overflow 대응 생략은 기존 합의를 유지한다. resize 재생성과 최소화 후 복원 대기는 S1-4에서 구현한다.
- 아직 이미지를 Acquire하거나 GPU 명령을 제출·Present하지 않는다. GPU 사용 완료 전 자원 정리 금지와 프레임 동기화는 S1-4에서 연결한다.


### S1-4 프레임 자원 중간 기록

- 현재 프레임 자원은 한 묶음으로 시작한다. `FrameResources`는 Graphics Family의 RESET_COMMAND_BUFFER Pool과 Primary Command Buffer 하나, signaled Fence를 구성한다. Pool·Fence는 RAII 소유, Command Buffer는 Pool에서 함께 해제한다.
- Fence 생성 직후 Adopt하고 상태를 조회하여 예외 발생 시 정리 공백을 없앴다. FrameResources는 main에서 Swapchain 이후에 생성되어 Context보다 먼저 정리된다.
- Agent는 Debug/Release 빌드를 확인했고 최종 수정 후 Debug 빌드도 확인했다. GPU 명령 제출과 실제 GUI 실행은 이번 작업에서 검증하지 않았다.
- 다음은 Acquire/Submit/Present를 연결할 binary semaphore의 생성·소유권 구성이다. 획득용은 프레임별, present 대기용은 Swapchain image별로 관리하며 반복 Acquire만 단독으로 수행하지 않는다.


### S1-4 Semaphore 준비 중간 기록

- binary semaphore용 RAII deleter와 공통 핸들 별칭을 추가했다. FrameResources마다 imageAvailable 하나, Swapchain의 실제 이미지마다 renderFinished 하나를 소유한다.
- 이미지별 semaphore는 빈 deque 원소를 먼저 생성하고 vkCreateSemaphore 성공 직후 Adopt한다. 현재 main의 FrameResources 인스턴스는 하나이며 두 슬롯 운용은 실제 프레임 루프 연결 시 추가한다.
- 사용자는 frame fence가 프레임 슬롯 재사용을 보호하고 imageAvailable 대기가 획득 이미지 접근 및 그 이미지의 renderFinished 재사용을 연결한다는 구분을 확인했다. Present 완료를 Graphics Fence만으로 판단하지 않는다.
- 오타 수정 후 Debug/Release 빌드 통과. 이번 변경의 실제 GUI 실행은 agent가 검증하지 않았다. 아직 Acquire·Submit·Present는 호출하지 않는다.
- 다음은 획득 이미지에 대한 layout 전환·Dynamic Rendering clear 명령 기록이며, 이후 두 프레임 슬롯의 Acquire–Submit–Present 루프를 연결한다.

### S1-4 Clear 명령 기록 중간 기록

- `Swapchain::GetImage`는 실제 이미지 목록의 borrowed handle을 반환한다. `RecordClearCommands`는 UNDEFINED → COLOR_ATTACHMENT_OPTIMAL 배리어, Dynamic Rendering의 CLEAR/STORE, PRESENT_SRC_KHR 전환을 기록한다.
- 배리어의 subresourceRange는 View와 같은 color aspect·mip 0 하나·array layer 0 하나다. attachment의 imageLayout은 전환 결과와 일치하며 renderingInfo는 layerCount 1과 Swapchain extent 전체를 사용한다.
- 사용자는 View의 접근 범위, 배리어의 동기화·레이아웃 전환 범위, renderArea의 픽셀 영역을 구분해 설명했다.
- Agent 검증: 최종 수정 후 임시 CMake 디렉터리에서 Debug 빌드 및 git diff --check 통과. 함수는 아직 호출하지 않으므로 GPU 실행·validation 검증은 미수행이다.
- S1-4는 계속 진행 중이다. 다음은 두 프레임 슬롯의 Acquire–Submit–Present 연결이며, 이후 resize·최소화·복원과 종료 처리를 검증한다.

### S1-4 Acquire–Submit–Present 중간 기록

- FrameResources 하나가 두 프레임 슬롯을 관리한다. Command Pool은 공유하고 Command Buffer·Fence·imageAvailable은 슬롯별로 분리한다. 해당 슬롯의 Fence를 기다린 뒤 그 Command Buffer만 reset하며, renderFinished는 획득한 Swapchain image index로 선택한다.
- PollEvents 후 framebuffer 크기를 조회한다. Acquire는 100ms timeout을 사용하며 TIMEOUT·NOT_READY는 같은 슬롯에서 재시도한다. OUT_OF_DATE 또는 0 크기에서는 루프를 종료한다. Acquire의 SUBOPTIMAL은 성공으로 받아 Submit·Present까지 진행한 후 종료한다.
- Dynamic Rendering clear 명령을 vkQueueSubmit2로 제출하고 vkQueuePresentKHR로 출력한다. imageAvailable 대기는 COLOR_ATTACHMENT_OUTPUT, renderFinished 신호는 ALL_COMMANDS를 사용한다. Fence reset은 명령 기록 후 제출 직전에 수행한다.
- 정상 종료와 루프 예외 경로 모두 FrameResources·Swapchain 소멸 전에 vkDeviceWaitIdle을 호출한다. 확장 없는 Vulkan에서 Present 자원 파괴의 엄밀한 완료 보장 문제는 남아 있으며, 재생성·종료 단계에서 다룬다.
- Agent 검증: 최종 수정 후 임시 CMake 디렉터리의 Debug 빌드 및 git diff --check 통과. 최종 코드의 배경색 출력과 validation 오류 해소는 아직 실행 증거가 없다.
- 다음은 실행 결과 확인과 크기 변경 이벤트 기반 Swapchain 재생성·최소화 후 복원이다. S1-4는 current 상태를 유지한다.


### S1-4 완료 기록 — 2026-09-15

- 두 프레임 슬롯의 Command Buffer·Fence·imageAvailable과 이미지별 renderFinished를 사용해 Acquire–Submit–Present 및 Dynamic Rendering clear를 구현했다. 제출 직전 Fence reset과 Acquire 재시도 경로를 구분한다.
- GLFW framebuffer 콜백은 크기와 변경 플래그를 갱신한다. 렌더링 루프에서 최신 Surface 지원 조건으로 Swapchain을 재생성하며, unique_ptr 교체와 oldSwapchain 전달로 Image View·이미지별 semaphore를 함께 교체한다. 0 크기에서는 이벤트를 기다린다.
- Acquire SUBOPTIMAL은 획득 성공으로 처리해 제출·Present까지 진행한다. Acquire OUT_OF_DATE와 Present OUT_OF_DATE·SUBOPTIMAL은 재생성을 요청한다. 설정 선택 실패 시 생성으로 진행하지 않는다.
- **채택한 정리 관행:** 재생성 및 정상·예외 종료 시 vkDeviceWaitIdle 후 기존 자원을 회수한다. 이는 확장 없는 Vulkan 애플리케이션에서 일반적으로 사용하는 방식이다. Device 제출 작업 대기와 Present 측 리소스 사용 완료의 엄밀한 보장은 구분하며, WaitIdle만으로 후자까지 명세상 보장한다고 주장하지 않는다. Present Fence를 제공하는 swapchain_maintenance1 도입은 이번 슬라이스 완료 조건에서 제외한다. 근거: [Khronos Swapchain Semaphore Reuse](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html).
- 검증: Agent가 최종 Debug/Release 빌드와 git diff --check를 확인했다. 사용자가 배경색 출력·validation 오류 해소·정상 종료와 resize 실행을 확인했다. Agent의 GUI 실행, 반복 횟수 기록, 최소화·복원 및 0 크기 경로의 별도 실행 증거는 없다. 해당 추가 실행 검증은 이번 완료 판단에서 유보하며 수행한 것으로 기록하지 않는다.
- 사용자 요청에 따라 위 관행과 검증 범위로 S1-4를 complete 처리한다. 다음은 S1-5 Graphics Pipeline과 2D 도형이다.


### S1-5 Shader 빌드 중간 기록

- GLSL 450 Vertex/Fragment Shader를 추가했다. Vertex Shader는 gl_VertexIndex로 내장 정점·색상 배열을 선택하고 location 0의 vec3 색상을 Fragment Shader에 전달한다.
- glslc를 Vulkan 필수 component로 검색하고 Vulkan 1.3 대상으로 두 SPIR-V를 빌드 디렉터리에 생성한다. 실행 파일이 shader target에 의존하며, RENDERER_SHADER_DIR 컴파일 정의에 출력 디렉터리를 전달한다. 로컬 빌드 경로 계약이며 배포용 경로 정책은 별도다.
- Agent가 두 SPIR-V 생성·Debug 빌드 및 무변경 재빌드 시 출력 timestamp 유지를 확인했다. 경로 정의의 누락된 $ 수정은 코드로 확인했고 사용자가 수정 후 빌드·셰이더 컴파일을 확인했다. 셰이더 변경 시 선택적 재빌드와 문법 오류 실패 검사는 별도로 수행하지 않았다.
- 다음은 SPIR-V 로딩과 Shader Module 생성·RAII 정리다. 아직 Graphics Pipeline·draw 연결은 없다.


### S1-5 Shader Module 중간 기록

- ShaderModule은 파일에서 uint32_t 배열로 SPIR-V를 읽고 vkCreateShaderModule 성공 직후 RAII 핸들에 Adopt한다. main에서 Device 생성 후 두 Module을 생성하며 Device보다 먼저 정리한다.
- 파일 크기는 양수·4의 배수 여부와 size_t/streamsize 표현 범위를 검사한다. uint32_t로 크기를 축소하지 않으며 전체 읽기 실패는 경로를 포함한 예외로 전달한다. 진단용 경로는 filesystem::path::string()으로 변환한다.
- 수정 후 Agent의 Debug/Release 빌드 통과. 실제 Module 생성·validation, 다른 작업 디렉터리 실행, 읽기 실패 주입은 이번 수정에서 수행하지 않았다. 다음은 Graphics Pipeline 생성이다.


### S1-5 삼각형 명령 연결 중간 기록

- GraphicsPipeline은 Pipeline Layout·Pipeline을 RAII 소유하고 생성 중 지역 ShaderModule을 사용한다. Dynamic Rendering의 color format 계약, 빈 vertex input, triangle list, dynamic viewport/scissor, blending·depth/stencil 비활성 상태를 구성했다.
- RecordFrameCommands는 BeginRendering과 EndRendering 사이에서 Pipeline bind·현재 Swapchain extent의 viewport/scissor·vkCmdDraw(3, 1, 0, 0)를 기록한다.
- Pipeline을 unique_ptr로 소유하며 Swapchain 재생성 전 Device 대기 이후 format이 바뀐 경우에만 Pipeline도 교체한다. extent 변경은 dynamic state 갱신으로 처리한다.
- Agent의 Debug/Release 빌드와 git diff --check 통과. 이번 draw 연결의 실제 삼각형 출력·resize·validation은 agent 미실행이며 사용자도 구체 실행 결과를 아직 명시하지 않았다.
- 사용자는 dynamic viewport로 extent 변경 시 Pipeline 재생성이 불필요한 이유와 Pipeline format 계약·실제 attachment View의 관계를 설명했다. 다음은 Push Constant를 통한 픽셀 단위 위치·크기·색상 전달이다.


### S1-5 Push Constant와 픽셀 좌표 중간 기록

- DrawPushConstants는 위치·크기, framebuffer 크기, 색상을 48바이트로 전달한다. CPU의 float 크기·멤버 offset·전체 크기를 static_assert로 확인하고 GLSL std430의 vec4 세 개와 맞췄다. Vertex stage의 Pipeline Layout 범위와 vkCmdPushConstants를 연결했다.
- 도형의 0~1 로컬 좌표에 위치·크기를 적용해 framebuffer 픽셀 좌표를 구하고, 현재 Swapchain extent로 NDC를 계산한다. 왼쪽 위 원점, 오른쪽 +X·아래쪽 +Y, framebuffer 픽셀 단위를 사용한다.
- 사용자 첨부 화면에서 단색 주황 삼각형 출력을 확인했다. 위치 (100,80), 크기 (240,180)에 따른 꼭짓점은 (220,80), (340,260), (100,260)이다. 사용자는 로컬 좌표와 양수 높이 viewport의 방향 관계를 설명했다.
- Agent의 최종 Debug 빌드와 git diff --check 통과. Resize 후 픽셀 크기 유지·validation 로그는 이번 단계에서 별도 실행 증거가 없다.
- 다음은 같은 좌표 계약으로 사각형 출력과 알파 블렌딩을 연결하는 작업이다.


### S1-5 완료 기록 — 2026-09-16

- GLSL→SPIR-V 자동 빌드, Shader Module·Pipeline Layout·Graphics Pipeline의 RAII, Dynamic Rendering draw를 연결했다. Dynamic viewport/scissor로 extent 변경에 대응하고 Swapchain format 변경 시 Pipeline을 재생성한다.
- 48바이트 Push Constant로 픽셀 위치·크기·viewport 크기·색상을 전달한다. CPU offset/size static_assert와 GLSL vec4 세 개의 배치를 맞췄다. 정점 0~2의 삼각형과 3~8의 사각형을 draw별 Push Constant로 그린다.
- Vertex→Fragment 색상을 vec4로 전달하고 straight alpha source-over blending을 활성화했다. Color attachment 배리어에 읽기 접근을 추가하고 같은 rendering 안에서 삼각형 후 사각형을 그린다.
- 사용자 첨부 화면으로 단색 삼각형, 빨간 삼각형과 반투명 파란 사각형의 겹침·배경 합성을 확인했다. 사용자는 창 크기 변경 후에도 이상 없음을 확인했다. 코드 검토에서 기능 오류를 발견하지 못했고, blending 구현의 Debug/Release 빌드와 git diff --check가 통과했다.
- 좌표 변환의 독립 예시: 800×600에서 (0,0)→(-1,-1), (400,300)→(0,0), (800,600)→(1,1). 위치 (100,80), 크기 (240,180)의 삼각형 꼭짓점은 (220,80), (340,260), (100,260)이다. 이는 수식에 의한 확인이며 GPU readback 검사는 수행하지 않았다.
- 검증 범위: Agent는 GUI를 직접 실행하지 않았다. 최신 단계의 validation 로그·format 변경 하드웨어 경로·다른 작업 디렉터리 실행·shader 오류 주입은 별도 증거가 없으며 통과로 기록하지 않는다. 사용자 완료 요청에 따라 현재 코드·빌드·화면·resize 증거로 S1-5를 complete 처리한다.
- 다음은 S1-6 Instancing과 최소 장면이다. S1-5 완료가 instance buffer 업로드·capacity·프레임별 입력 수명 구현을 포함하지는 않는다.


### S1-6 첫 작업 — Buffer와 Memory 기반

- 첫 단계는 VkBuffer·VkDeviceMemory 생성, 바인딩, host-visible/coherent 메모리 매핑과 범위 검사된 CPU 쓰기다. 이 단계에서는 GPU draw 입력 연결과 프레임별 갱신을 추가하지 않는다.
- 공통 정점은 우선 셰이더 배열에 유지하고 개별 위치·크기·색상을 instance-rate vertex input으로 옮기는 순서로 진행한다. viewport 크기는 프레임 공통 데이터다.
- Buffer 하나당 별도 메모리 할당으로 시작하고 HOST_VISIBLE | HOST_COHERENT 타입을 요구한다. 대응 타입이 없으면 명시적 실패로 처리한다. GPU 사용 중 덮어쓰기 방지는 다음 프레임 슬롯 연결 단계에서 다룬다.


### S1-6 HostVisibleBuffer 중간 기록

- HostVisibleBuffer가 Vertex Buffer와 전용 DeviceMemory를 RAII 소유한다. memoryTypeBits와 HOST_VISIBLE | HOST_COHERENT 조건으로 메모리 타입을 선택하고 requirements.size로 할당한 뒤 offset 0에 바인딩한다. 생성자 마지막에 map하고 소멸 시 Unmap → Buffer 파괴 → Memory 해제 순서로 정리한다.
- Write는 0바이트를 허용하고 capacity 초과 및 양수 크기의 null 입력을 복사 전에 거부한다. GPU 사용 중 덮어쓰기 방지는 호출자의 전제조건이며 coherent 메모리가 실행 동기화를 대신하지 않는다.
- Agent의 Debug/Release 빌드와 git diff --check 통과. 사용자가 주석 추가 후 실행을 확인했다. main의 임시 스코프는 생성·Write·소멸만 수행하며 GPU draw에는 아직 연결하지 않는다. Capacity 초과 호출 및 실패 주입은 별도 실행 증거가 없다.
- 다음은 FIF별 Instance Buffer와 instance-rate vertex input 연결이다. S1-6는 current 상태를 유지한다.


### S1-6 Instancing과 Renderer2D 분리 중간 기록

- 32바이트 InstanceData의 위치·크기와 색상을 instance-rate vertex input으로 읽고, 16바이트 Push Constant에는 viewport 크기만 전달한다. 슬롯별 Fence 대기 후 같은 슬롯 Buffer를 갱신한다. 0개는 배경 렌더링을 유지하고 용량 초과는 업로드 전에 거부한다.
- Renderer2D가 Pipeline과 FIF별 Instance Buffer·업로드 개수를 소유하며 UpdateInstance와 RecordDraws로 업로드·draw를 담당한다. FrameResources는 Command Buffer·Fence·imageAvailable을 관리한다. main은 Acquire/Submit/Present와 rendering 범위·배리어를 유지한다.
- Renderer 생성 시 양수 슬롯 수·capacity를 요구하고, 업로드 및 draw에서 슬롯 범위를 검사한다. format 변경은 새 Pipeline 생성·교체 성공 후 상태를 갱신하며 동일 format 요청은 생략한다.
- Agent의 최종 Debug/Release 빌드와 git diff --check 통과. 사용자가 Renderer 분리 후 실행을 확인했다. 0·최대·초과 각각의 최종 실행 로그와 Pipeline 생성 실패 주입은 별도 증거가 없다.
- Resize 드래그 중 기존 화면이 늘어나다 종료 후 픽셀 크기로 복귀하는 현상은 이벤트 처리 지연 가능성을 확인했다. GLFW PollEvents의 live resize 중 지연이 유력하지만 계측으로 확정하지는 않았다. 현재 픽셀 좌표 계약은 유지한다.
- 다음은 instance 종류를 추가하여 사각형 영역의 fragment를 원 모양으로 제한하는 작업이다. 선·궤적은 후속 작업으로 남긴다.


### S1-6 원·선분 표현 중간 기록 — 2026-09-17

- InstanceData에 Shape 구분을 추가하여 사각형·원/타원을 같은 draw에서 처리한다. Fragment Shader는 보간한 로컬 좌표로 원 밖을 discard한다. 종류 값은 R32_UINT 입력과 flat 출력으로 전달한다.
- Line은 positionAndSize를 시작점·끝점으로 해석하고 offset 36의 thickness를 사용한다. 전체 stride는 48바이트를 유지한다. Vertex Shader에서 픽셀 공간의 방향·수직 벡터로 두께 있는 사각형을 구성하며, 양 끝은 평평하게 끝난다.
- 초기화 전 출력 변수를 읽던 분기를 instanceShape 입력으로 수정하고 Renderer의 허용 종류에 Line을 추가했다. Agent의 최종 Debug 빌드와 git diff --check 통과. 사용자가 실행을 확인했다.
- **사용자 결정으로 보류:** shaderDemoteToHelperInvocation 지원 확인·활성화와 Line 입력의 유한성·양수 두께·서로 다른 끝점 검사를 이번 커밋에서 추가하지 않는다.
- 현재 glslc의 Vulkan 1.3 대상 Fragment SPIR-V는 DemoteToHelperInvocation capability를 선언하지만 Device에서 해당 feature를 활성화하지 않아 보고된 validation 오류가 남는다. 실행 확인은 해당 오류 해소를 의미하지 않는다. 원 추가 커밋과 현재 Fragment 소스가 동일하며 두 소스를 재컴파일해 같은 capability를 확인했다.
- Line 입력 검사를 보류했으므로 현재는 호출자가 유효한 좌표·양수 두께·서로 다른 끝점을 제공해야 한다. 길이 0인 선분의 normalize 결과에 의존하지 않는다.
- 다음은 점 목록을 인접한 선분으로 변환하는 궤적과 최소 장면 구성이다. S1-6는 current 상태를 유지한다.


### S1-6 궤적과 최소 장면 중간 기록 — 2026-09-17

- BuildTrailInstances는 인접한 점들을 Line instance로 바꾸고 동일한 인접 점은 생략한다. 점 0·1개는 빈 결과다. TrailInstance는 최근 64개 중심점을 보관하여 최대 63개 선분을 생성한다.
- 첫 번째 사각형의 중심을 기록하고 매 프레임 궤적→장면 도형 순으로 최종 목록을 구성한다. 빈 장면에서도 Renderer를 갱신하여 이전 슬롯의 draw 개수가 남지 않게 했다. 빈 장면에서는 궤적도 그리지 않지만 CPU 점 기록은 유지한다.
- 사용자 화면에서 고정 3개 선분과 이동 도형 뒤의 궤적을 확인했다. 현재 이동은 반복당 1픽셀이므로 꽉 찬 궤적의 길이는 약 63픽셀이며 시간 기반 속도·샘플링은 구현하지 않았다.
- Agent가 실제 CPU helper를 추출한 임시 검사로 0·1개 점, 중복 점 생략, 선분 끝점, 64개 기록 제한과 최대 63개 선분을 확인했다. 최종 수정 후 Debug/Release 빌드와 git diff --check 통과. 최종 순서·빈 장면 수정 이후 GUI 실행은 Agent 미수행이다.
- 다음은 최종 장면의 resize·최소화·복원·종료와 0·최대·초과 instance 경계 검증 및 문서 완료 정리다. 기존 shaderDemoteToHelperInvocation 미활성화와 Line 입력 검사 보류 결정은 유지하며 validation 무오류로 기록하지 않는다. S1-6는 current 상태다.


## S1-6 및 튜토리얼 완료 — 2026-09-17

- 사용자 요청에 따라 S1-1~S1-6 전부 complete 처리한다. 완료 범위는 Graphics Pipeline 기반 2D 렌더러 학습이며 Vulkan 전체 기능이나 GPU 계산 백엔드 구현 완료를 의미하지 않는다.
- 사용자 제공 화면으로 사각형·원/타원·선분·고정/이동 궤적을 확인했다. 사용자는 최종 실행 확인 항목(resize·최소화·복원·종료)과 기존 보류 메시지 외 새 validation 문제 없음을 확인했다.
- Agent는 최종 소스의 임시 복사본에 테스트 입력과 계측만 추가해 실제 Renderer·GPU 경로를 Debug로 실행했다. 0·1·256개 각각 8프레임 동안 업로드·draw 개수와 정상 종료를 확인했다. 257개는 업로드·draw 전에 capacity 예외 및 종료 코드 1을 확인했다. 3개 4프레임→0개 4프레임 전환에서 두 FIF 슬롯 모두 개수가 0으로 갱신됐다. 픽셀 readback 검사는 하지 않았다.
- 기존 CPU helper 검사에서 빈/단일/중복 점, 선분 끝점과 64점·63선분 제한을 확인했다. 최종 구현의 Debug/Release 빌드가 통과했다. 임시 실행 검사 후 파일을 정리했으며 제품 코드는 변경하지 않았다.
- 검증 환경: macOS, Apple M4 Pro 사용자 환경, AppleClang 16, GLFW 3.4.0, Vulkan SDK/loader 1.4.335 계열, 앱 대상 Vulkan 1.3. 다른 OS·분리 Graphics/Present Queue 하드웨어는 검증하지 않았다.

### 채택한 관행과 명시적 보류

- vkDeviceWaitIdle 후 Swapchain·Present 자원을 회수하는 일반적인 관행을 채택했다. Present 자원 사용 완료를 명세상 엄밀하게 확인하는 maintenance 확장의 Present Fence는 미도입이다.
- shaderDemoteToHelperInvocation의 지원 확인·활성화는 사용자 결정으로 보류했다. 실제 최종 경계 실행에서도 DemoteToHelperInvocation capability 관련 validation 메시지가 재현됐다. 의도한 Debug callback 테스트 메시지와 이 알려진 오류 외 새 validation 메시지는 없었다. Validation 무오류 상태로 기록하지 않는다.
- Line 입력의 유한성·양수 두께·서로 다른 끝점 검사는 사용자 결정으로 보류했다. 호출자는 유효한 값을 제공한다. 궤적 생성기는 같은 인접 점을 건너뛴다.
- 이동은 반복당 고정 거리이며 궤적은 최근 64점이다. 시간 기반 이동·샘플링, 선분 join·경계 안티앨리어싱, live resize 중 갱신 개선, 성능 검증은 완료 범위 밖이다.

### 빌드·실행

저장소 루트에서:

```sh
cmake -S src/vulkan/renderer_project -B src/vulkan/renderer_project/build/dev -DCMAKE_BUILD_TYPE=Debug
cmake --build src/vulkan/renderer_project/build/dev
./src/vulkan/renderer_project/build/dev/vulkan_renderer
```

셰이더는 glslc로 Vulkan 1.3 대상 SPIR-V를 빌드 디렉터리에 생성한다. RENDERER_SHADER_DIR은 절대 빌드 경로이며 현재 실행 계약은 로컬 빌드용이다.

### 후속 범위

Vulkan Client의 실제 입력·상태 연결, Compute Pipeline·GPU 계산과 Graphics 간 데이터/동기화 연결은 부모 #5에서 별도 계획 후 진행한다. 이번 완료 처리로 부모 #5 전체를 완료하지 않는다.
