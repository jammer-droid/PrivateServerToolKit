#pragma once

#include <pstk/service_host/TkServiceHost.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace pstk::service
{
namespace detail
{

// Packet 검증 기본 템플릿
template <typename Packet, typename Check = void> struct PacketContract final
{
    static constexpr bool is_valid = false;
};

// Packet 검증 부분 특수화
// - <Packet>만 사용자가 전달하고, 두 번째 타입은 컴파일 타임 검사로 결정
// - 검사 실패시 특수화 인스턴스는 실패하고 기본 템플릿 사용
// - std::declval<Packet&>() 으로 Packet& 이 있다고 가정하고 함수 호출이 가능한지 검증
// - decltype으로 타입을 확인할 수 있으면 검증 통과
template <typename Packet>
struct PacketContract<Packet, std::void_t<decltype(Packet::PacketId), decltype(Packet::PayloadBytes),
                                          decltype(std::declval<Packet &>().Decode(std::declval<TkByteView>(),
                                                                                   TkDiagnosticCallbackInfo{})),
                                          decltype(std::declval<const Packet &>().Encode(
                                              std::declval<TkMutableByteView>(), TkDiagnosticCallbackInfo{}))>>
    final
{
    // 얻어낸 타입이 원하는 타입과 일치하는지 검증
    static constexpr bool is_valid =
        std::is_default_constructible<Packet>::value && std::is_destructible<Packet>::value &&
        std::is_same<typename std::remove_cv<decltype(Packet::PacketId)>::type, std::uint16_t>::value &&
        std::is_same<typename std::remove_cv<decltype(Packet::PayloadBytes)>::type, std::size_t>::value &&
        std::is_same<decltype(std::declval<Packet &>().Decode(std::declval<TkByteView>(), TkDiagnosticCallbackInfo{})),
                     TkResult>::value &&
        std::is_same<decltype(std::declval<const Packet &>().Encode(std::declval<TkMutableByteView>(),
                                                                    TkDiagnosticCallbackInfo{})),
                     TkResult>::value;
};

// OneWayHandler의 Packet 계약과 Handler 호출 형식 검증
// TkResult (Service::*) (const TkServiceContext&, const Request &) noexcept
//  - Service 타입에 속한 멤버 함수 포인터
template <typename Service, typename Request, auto Handler>
inline constexpr bool IsSupportedOneWayHandler =
    PacketContract<Request>::is_valid &&
    std::is_same<decltype(Handler), TkResult (Service::*)(const TkServiceContext &, const Request &) noexcept>::value;

// RequestResponse의 Packet 계약과 Handler 호출 형식 검증
template <typename Service, typename Request, typename Response, auto Handler>
inline constexpr bool IsSupportedRequestResponseHandler =
    PacketContract<Request>::is_valid && PacketContract<Response>::is_valid &&
    std::is_same<decltype(Handler),
                 TkResult (Service::*)(const TkServiceContext &, const Request &, Response *) noexcept>::value;

template <typename Packet>
TkResult DecodeRequest(TkByteView payload, void *const requestStorage, TkDiagnosticCallbackInfo diagnostic) noexcept
{
    static_assert(PacketContract<Packet>::is_valid, "Request must provide generated packet metadata and codec");

    Packet *const request = ::new (requestStorage) Packet{};
    const TkResult result = request->Decode(payload, diagnostic);
    if (result != TK_SUCCESS)
    {
        request->~Packet();
    }

    return result;
}

template <typename Packet> void DestroyPacket(void *const storage) noexcept
{
    static_assert(PacketContract<Packet>::is_valid, "Packet must provide generated packet metadata and codec");
    static_cast<Packet *>(storage)->~Packet();
}

template <typename Service, typename Request, auto Handler>
TkResult InvokeOneWay(void *const serviceInstance, const TkServiceContext *const context,
                      const void *const requestStorage) noexcept
{
    static_assert(IsSupportedOneWayHandler<Service, Request, Handler>,
                  "Handler must be a noexcept one-way service member function");
    return (static_cast<Service *>(serviceInstance)->*Handler)(*context, *static_cast<const Request *>(requestStorage));
}

template <typename Service, typename Request, typename Response, auto Handler>
TkResult InvokeRequestResponse(void *const serviceInstance, const TkServiceContext *const context,
                               const void *const requestStorage, void *const responseStorage) noexcept
{
    static_assert(IsSupportedRequestResponseHandler<Service, Request, Response, Handler>,
                  "Handler must be a noexcept request-response service member function");

    Response *const response = ::new (responseStorage) Response{};
    const TkResult result = (static_cast<Service *>(serviceInstance)->*Handler)(
        *context, *static_cast<const Request *>(requestStorage), response);
    if (result != TK_SUCCESS)
    {
        response->~Response();
    }

    return result;
}

template <typename Response>
TkResult EncodeResponse(const void *const responseStorage, const TkMutableByteView output,
                        TkDiagnosticCallbackInfo diagnostic) noexcept
{
    static_assert(PacketContract<Response>::is_valid, "Response must provide generated packet metadata and codec");
    return static_cast<const Response *>(responseStorage)->Encode(output, diagnostic);
}

// 템플릿 파라미터 팩 {BindingA, BindingB, BindingC ...}
template <typename Service, typename... Bindings> struct SameService;

// 여러 binding이 같은 Service 타입을 사용하는지 검증
// <Service, First=BindingA, Rest...= BindingB, BindingC ...>
template <typename Service, typename First, typename... Rest> struct SameService<Service, First, Rest...> final
{
    static constexpr bool is_valid = std::is_same<Service, typename First::service_type>::value &&
                                     (std::is_same<Service, typename Rest::service_type>::value && ...);
};
} // namespace detail

template <typename Service, typename Request, auto Handler> struct OneWayBinding final
{
    using service_type = Service;
    using request_type = Request;

    static constexpr bool is_valid = detail::IsSupportedOneWayHandler<Service, Request, Handler>;

    static_assert(is_valid, "BindOneWay requires a generated Request and a noexcept handler signature");

    static TkServiceBindingInfo Describe() noexcept
    {
        TkServiceBindingInfo info{};
        info.type = TK_BINDING_TYPE_ONE_WAY;
        info.request.packetId = static_cast<std::uint16_t>(Request::PacketId);
        info.request.payloadBytes = static_cast<std::size_t>(Request::PayloadBytes);
        info.request.requestSize = sizeof(Request);
        info.request.requestAlignment = alignof(Request);
        info.request.decodeRequest = &detail::DecodeRequest<Request>;
        info.request.destroyRequest = &detail::DestroyPacket<Request>;
        info.operation.oneWay.invokeHandler = &detail::InvokeOneWay<Service, Request, Handler>;
        return info;
    }
};

template <typename Service, typename Request, typename Response, auto Handler> struct RequestResponseBinding final
{
    using service_type = Service;
    using request_type = Request;
    using response_type = Response;

    static constexpr bool is_valid = detail::IsSupportedRequestResponseHandler<Service, Request, Response, Handler>;

    static_assert(is_valid, "BindRequestResponse requires generated packets and a noexcept handler signature");

    static TkServiceBindingInfo Describe() noexcept
    {
        TkServiceBindingInfo info{};
        info.type = TK_BINDING_TYPE_REQUEST_RESPONSE;
        info.request.packetId = static_cast<std::uint16_t>(Request::PacketId);
        info.request.payloadBytes = static_cast<std::size_t>(Request::PayloadBytes);
        info.request.requestSize = sizeof(Request);
        info.request.requestAlignment = alignof(Request);
        info.request.decodeRequest = &detail::DecodeRequest<Request>;
        info.request.destroyRequest = &detail::DestroyPacket<Request>;
        info.operation.requestResponse.packetId = static_cast<std::uint16_t>(Response::PacketId);
        info.operation.requestResponse.payloadBytes = static_cast<std::size_t>(Response::PayloadBytes);
        info.operation.requestResponse.responseSize = sizeof(Response);
        info.operation.requestResponse.responseAlignment = alignof(Response);
        info.operation.requestResponse.invokeHandler =
            &detail::InvokeRequestResponse<Service, Request, Response, Handler>;
        info.operation.requestResponse.encodeResponse = &detail::EncodeResponse<Response>;
        info.operation.requestResponse.destroyResponse = &detail::DestroyPacket<Response>;
        return info;
    }
};

template <typename Service, typename Request, auto Handler>
constexpr OneWayBinding<Service, Request, Handler> BindOneWay() noexcept
{
    return {};
}

/*
 * Service: 서비스 타입
 * Request: 요청 타입
 * Response: 응답 타입
 * Handler: 호출할 함수
 */

template <typename Service, typename Request, typename Response, auto Handler>
constexpr RequestResponseBinding<Service, Request, Response, Handler> BindRequestResponse() noexcept
{
    return {};
}

template <typename First, typename... Rest> struct BindingSet final
{
    using service_type = typename First::service_type;
    static constexpr bool is_valid = detail::SameService<service_type, First, Rest...>::is_valid;

    static_assert(is_valid, "all bindings in a service binding set must use the same service type");

    static constexpr std::size_t binding_count = 1U + sizeof...(Rest);

    constexpr BindingSet(const First &, const Rest &...) noexcept
    {
    }

    void FillDescriptors(std::array<TkServiceBindingInfo, binding_count> &outDescriptors) const noexcept
    {
        std::size_t index = 0U;
        outDescriptors[index++] = First::Describe();
        ((outDescriptors[index++] = Rest::Describe()), ...);
    }

    constexpr std::size_t size() const noexcept
    {
        return binding_count;
    }
};

template <typename First, typename... Rest>
constexpr BindingSet<First, Rest...> MakeBindings(const First &first, const Rest &...rest) noexcept
{
    return {first, rest...};
}

template <typename Service, std::size_t MiddlewareCount, typename First, typename... Rest>
TkResult RegisterService(TkServiceHost *const host, Service &service, const TkServiceExecutorCallbackInfo executor,
                         const std::array<TkServiceMiddlewareInfo, MiddlewareCount> &middlewares,
                         const BindingSet<First, Rest...> &bindings,
                         const TkDiagnosticCallbackInfo diagnostic = {}) noexcept
{
    static_assert(std::is_same<Service, typename BindingSet<First, Rest...>::service_type>::value,
                  "service instance type must match every binding");

    std::array<TkServiceBindingInfo, BindingSet<First, Rest...>::binding_count> descriptors{};
    bindings.FillDescriptors(descriptors);
    const TkServiceRegistrationInfo registration = {static_cast<void *>(&service),
                                                    executor,
                                                    middlewares.data(),
                                                    MiddlewareCount,
                                                    descriptors.data(),
                                                    descriptors.size()};
    return TkServiceHostRegisterService(host, &registration, diagnostic);
}

} // namespace pstk::service
