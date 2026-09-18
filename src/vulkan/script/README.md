# Shader compilation

`compile_shaders.py`는 `glslc`를 호출해 셰이더 파일 또는 디렉터리를 SPIR-V로 컴파일한다. Python 3와 `glslc`가 필요하다.

```sh
python3 compile_shaders.py -s ../vulkan_runtime/shaders -o /tmp/builtin-shaders
python3 compile_shaders.py -s path/to/example.comp -o /tmp/user-shaders --target-env vulkan1.2
```

- `-s`: 소스 파일 또는 디렉터리. 디렉터리는 하위 경로까지 검색한다.
- `-o`: 출력 디렉터리. `example.vert`는 `example.vert.spv`가 되며 하위 경로는 유지한다.
- `--target-env`: 생략하면 `vulkan1.3`. 지정한 값은 `glslc`에 전달한다.
- `--glslc`: 선택적인 컴파일러 경로. 기본은 PATH의 `glslc`이며 CMake는 발견한 도구 경로를 전달한다.

디렉터리 모드는 `.vert`, `.frag`, `.comp`, `.geom`, `.tesc`, `.tese`, `.mesh`, `.task`와 ray-tracing stage 확장자를 처리한다. include용 파일은 단독 컴파일하지 않는다. 실행할 때마다 소스를 컴파일하므로 include 파일 변경도 반영하며, 결과가 같으면 출력 파일을 다시 쓰지 않는다. 실패하면 0이 아닌 종료 코드를 반환하고 해당 파일의 기존 결과는 보존한다. 전체 파일 묶음의 원자적 교체나 삭제된 소스의 이전 SPIR-V 자동 삭제는 제공하지 않는다.

## Build integration

- runtime의 `dev` build preset은 `vulkan_runtime_assets`를 빌드한다. 라이브러리 빌드 이후 builtin을 `build/dev/shaders/runtime`에 컴파일하고 설정된 install prefix의 `shaders/runtime`에 복사한다.
- 이후 `cmake --install build/dev`가 라이브러리·헤더·builtin SPIR-V·스크립트·CMake package를 함께 설치한다. 개발 preset의 install prefix는 `build/dev/sdk`다.
- 앱의 `dev` build preset은 `vulkan_app_assets`를 빌드한다. 실행 파일 빌드 이후 `vulkan_app/shaders`의 소스를 설치된 SDK의 `shaders/app`에 컴파일한다. SDK builtin은 실행 파일 옆 `shaders/runtime`에도 복사한다.
- 앱별 소스·출력은 `APP_USER_SHADER_SOURCE_DIR`, `APP_USER_SHADER_OUTPUT_DIR` cache 변수로 변경할 수 있다. 여러 앱이 SDK를 공유하면 출력 하위 디렉터리를 앱별로 구분한다. SDK에 쓰기 권한이 필요하다.
- 라이브러리나 실행 파일 타깃만 직접 빌드하면 asset 단계는 실행되지 않는다. 셰이더까지 준비하려면 위 preset이나 `*_assets` 타깃을 사용한다.
- 사용자 셰이더가 없는 빈 소스 디렉터리는 정상 처리한다. 현재 앱 코드에 사용자 셰이더 로딩 API를 추가하는 작업은 포함하지 않는다.
