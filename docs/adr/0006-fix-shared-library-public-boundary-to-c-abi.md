# Shared library의 public binary 경계를 C ABI로 고정한다

PrivateServerToolKit의 compiled tool과 runtime component는 기본적으로 DLL, `.so` 또는 `.dylib` 형태의 shared library로 제공하고, 그 public binary 경계는 `extern "C"` 함수와 fixed-width scalar, C-compatible POD, function pointer 및 opaque handle로 고정한다. STL, exception, RTTI, template과 compiler-dependent C++ class layout은 shared-library ABI에 노출하지 않으며, 한 module에서 생성한 memory와 handle은 같은 module의 public API로 해제한다.

C++ consumer의 사용성을 포기한다는 뜻은 아니다. Template, type traits, macro와 RAII가 필요한 typed convenience layer는 consumer binary에 compile되는 C++17 facade로 제공하고, facade는 안정적인 C ABI를 통해 shared runtime을 호출한다. Facade에는 library의 shared mutable state, scheduling policy와 version-dependent 핵심 동작을 두지 않는다. Generated C++ DTO와 codec처럼 shared-library symbol을 직접 구성하지 않는 source contract도 이 binary ABI 결정과 구분한다.

Compiled component의 기본 배포는 shared library지만 header-only Common과 binary 밖의 generated support, shared runtime의 내부 구현을 나누는 static library는 예외다. 새로운 component에서 제한된 C++ ABI와 Pimpl을 public binary 경계의 대안으로 다시 선택하지 않으며, 이 정책을 바꾸려면 consumer/toolchain compatibility와 migration 영향을 검토한 후 이 ADR을 대체한다.
