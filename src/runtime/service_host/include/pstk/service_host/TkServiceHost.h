#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pstk/TkByteView.h>
#include <pstk/TkDiagnostic.h>
#include <pstk/TkResult.h>

#include "pstk_service_host_export.h"

typedef struct TkServiceHost TkServiceHost;
typedef struct TkServiceJob TkServiceJob;

typedef struct TkHostConnectionKey
{
    uint64_t connectionId;
    uint64_t generation;
} TkHostConnectionKey;

typedef struct TkHostOutputInfo
{
    TkHostConnectionKey connectionKey;
    uint16_t packetId;
    TkByteView payload;
} TkHostOutputInfo;

typedef TkResult (*TkHostOutputCallback)(const TkHostOutputInfo *outputInfo, void *userData);

typedef struct TkHostOutputCallbackInfo
{
    TkHostOutputCallback callback;
    void *userData;
} TkHostOutputCallbackInfo;

typedef struct TkServiceHostCreateInfo
{
    TkHostOutputCallbackInfo outputAdapter;
} TkServiceHostCreateInfo;

typedef TkResult (*TkServiceJobSubmitCallback)(TkServiceJob *job, void *userData);

typedef struct TkServiceExecutorCallbackInfo
{
    TkServiceJobSubmitCallback callback;
    void *userData;
} TkServiceExecutorCallbackInfo;

typedef struct TkServiceMiddlewareCallInfo
{
    TkHostConnectionKey connectionKey;
    uint16_t packetId;
} TkServiceMiddlewareCallInfo;

typedef TkResult (*TkServiceMiddlewareCallback)(const TkServiceMiddlewareCallInfo *callInfo,
                                                TkDiagnosticCallbackInfo diagnostic, void *userData);

typedef struct TkServiceMiddlewareInfo
{
    TkServiceMiddlewareCallback callback;
    void *userData;
} TkServiceMiddlewareInfo;

typedef struct TkServiceContext
{
    TkHostConnectionKey connectionKey;
} TkServiceContext;

typedef TkResult (*TkRequestDecodeCallback)(TkByteView payload, void *requestStorage,
                                            TkDiagnosticCallbackInfo diagnostic);

typedef void (*TkRequestDestroyCallback)(void *requestStorage);

typedef TkResult (*TkOneWayHandlerCallback)(void *serviceInstance, const TkServiceContext *context,
                                            const void *requestStorage);

typedef TkResult (*TkResponseHandlerCallback)(void *serviceInstance, const TkServiceContext *context,
                                              const void *requestStorage, void *responseStorage);

typedef TkResult (*TkResponseEncodeCallback)(const void *responseStorage, TkMutableByteView output,
                                             TkDiagnosticCallbackInfo diagnostic);

typedef void (*TkResponseDestroyCallback)(void *responseStorage);

typedef enum TkBindingType
{
    TK_BINDING_TYPE_ONE_WAY = 1,
    TK_BINDING_TYPE_REQUEST_RESPONSE = 2
} TkBindingType;

typedef struct TkServiceRequestBindingInfo
{
    uint16_t packetId;
    size_t payloadBytes;
    size_t requestSize;
    size_t requestAlignment;

    TkRequestDecodeCallback decodeRequest;
    TkRequestDestroyCallback destroyRequest;
} TkServiceRequestBindingInfo;

typedef struct TkOneWayBindingInfo
{
    TkOneWayHandlerCallback invokeHandler;
} TkOneWayBindingInfo;

typedef struct TkRequestResponseBindingInfo
{
    uint16_t packetId;
    size_t payloadBytes;
    size_t responseSize;
    size_t responseAlignment;
    TkResponseHandlerCallback invokeHandler;
    TkResponseEncodeCallback encodeResponse;
    TkResponseDestroyCallback destroyResponse;
} TkRequestResponseBindingInfo;

typedef union TkServiceBindingOperation {
    TkOneWayBindingInfo oneWay;
    TkRequestResponseBindingInfo requestResponse;
} TkServiceBindingOperation;

typedef struct TkServiceBindingInfo
{
    TkBindingType type;
    TkServiceRequestBindingInfo request;
    TkServiceBindingOperation operation;
} TkServiceBindingInfo;

typedef struct TkServiceRegistrationInfo
{
    void *serviceInstance;
    TkServiceExecutorCallbackInfo executor;

    const TkServiceMiddlewareInfo *middlewares;
    size_t middlewareCount;

    const TkServiceBindingInfo *bindings;
    size_t bindingCount;
} TkServiceRegistrationInfo;

#ifdef __cplusplus
extern "C"
{
#endif

    PSTK_SERVICE_HOST_API TkResult TkServiceHostGetApiVersion(uint32_t *outVersion);

    PSTK_SERVICE_HOST_API TkResult TkServiceHostCreate(const TkServiceHostCreateInfo *createInfo,
                                                       TkServiceHost **outHost);

    PSTK_SERVICE_HOST_API TkResult TkServiceHostRegisterService(TkServiceHost *host,
                                                                const TkServiceRegistrationInfo *registration,
                                                                TkDiagnosticCallbackInfo diagnostic);

    PSTK_SERVICE_HOST_API TkResult TkServiceHostFinalizeRegistration(TkServiceHost *host,
                                                                     TkDiagnosticCallbackInfo diagnostic);

    PSTK_SERVICE_HOST_API TkResult TkServiceHostProcessPacket(TkServiceHost *host, TkHostConnectionKey connectionKey,
                                                              uint16_t packetId, TkByteView payload,
                                                              TkDiagnosticCallbackInfo diagnostic);

    PSTK_SERVICE_HOST_API void TkServiceHostDestroy(TkServiceHost *host);

    PSTK_SERVICE_HOST_API TkResult TkServiceJobExecute(TkServiceJob *job, TkDiagnosticCallbackInfo diagnostic);

    PSTK_SERVICE_HOST_API void TkServiceJobDestroy(TkServiceJob *job);

#ifdef __cplusplus
}
#endif
