# Issue #6: Vulkan 2D 렌더러 기반 구축과 단계별 학습

- Issue: <https://github.com/jammer-droid/PrivateServerToolKit/issues/6>
- Parent: [#5 Vulkan World Lab](https://github.com/jammer-droid/PrivateServerToolKit/issues/5)의 S1
- 상태: 학습 계획 수립, 구현 완료 0/6. S1-1 current, S1-2 next, S1-3~S1-6 pending.
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
- 기존 Compute 실습은 보존한다. 새 실행 경로의 타깃 이름·파일 배치는 S1-1 안내에서 정하고 기존 실습을 덮어쓰지 않는다.

## 단계별 계약

### S1-1 — 실행 골격과 Instance

- **선행:** 없음. 현재 단계이며 첫 실험은 Instance 생성·정상 파괴다.
- **결과/seam:** `src/vulkan/CMakeLists.txt`와 새 Graphics 실행 진입점. 버전·extension·layer 조회, Instance와 debug messenger 생성·정리.
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
| S1-1 | Graphics 타깃·파일 배치, 실패 처리 방식, SDK·첫 검증 환경. 현재 macOS/MoltenVK·C++17·C Vulkan API를 출발 후보로 삼는다. API 최소 버전은 실제 지원과 함께 확정한다. |
| S1-2 | 창 라이브러리(GLFW 후보), Graphics/Present 분리 family 지원 정책과 활성화 feature. |
| S1-3~4 | Present mode, frame slot 수, Dynamic Rendering 또는 Render Pass. 기존 Vulkan 1.3 요청을 참고하되 지원 확인 없이 기능 사용을 확정하지 않는다. |
| S1-5 | Shader 언어·컴파일러, 좌표 원점·축·단위, blending과 그리기 순서. |
| S1-6 | 원·선·궤적 표현, instance 데이터와 capacity 정책. |

디버그 UI는 필수 선행 조건이 아니다. 필요성이 생길 때 선택하며 창 이벤트만으로 초기 학습을 시작한다. 추가 플랫폼 검증과 성능 계측은 자동으로 확대하지 않는다.

## 참고 자료

1. [Khronos Tutorial](https://docs.vulkan.org/tutorial/latest/00_Introduction.html) — 개념/실습. 초기화에서 Graphics 출력까지의 흐름을 참고한다. 확인한 최신판은 Vulkan 1.4·C++20·Vulkan-Hpp·Slang 기준이므로 현재 코드에 그대로 복사하지 않는다.
2. [Khronos Instancing](https://docs.vulkan.org/samples/latest/samples/api/instancing/README.html) — S1-6 실습. 같은 mesh에 개별 instance 입력을 주는 방식을 참고한다.
3. [Khronos Swapchain Semaphore Reuse](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html) — S1-4 정확성 reference. submission 완료와 presentation semaphore 재사용 조건을 구분한다.

## 검증과 진행 기록

프레임워크 초기화·출력은 실제 실행과 validation을 주된 증거로 사용한다. 단순 래퍼별 mock test를 추가하지 않는다. 좌표 변환 등 독립 expected result가 있는 로직은 필요할 때 focused test로 검증한다. 장시간 stress·성능 benchmark는 포함하지 않는다.

| 단계 | 상태 | 구현/실행 증거 | 이해 확인 |
|---|---|---|---|
| S1-1 | current | 아직 없음 | 아직 없음 |
| S1-2 | next | 아직 없음 | 아직 없음 |
| S1-3 | pending | 아직 없음 | 아직 없음 |
| S1-4 | pending | 아직 없음 | 아직 없음 |
| S1-5 | pending | 아직 없음 | 아직 없음 |
| S1-6 | pending | 아직 없음 | 아직 없음 |

현재 다음 행동은 S1-1의 실행 골격과 Instance 생성·파괴를 안내하는 것이다. 이 문서 작성은 해당 코드 구현을 대신하지 않는다. 후속 진행도·학습 기록은 별도 단계 문서 없이 이 문서의 stable-ID section을 갱신한다.
