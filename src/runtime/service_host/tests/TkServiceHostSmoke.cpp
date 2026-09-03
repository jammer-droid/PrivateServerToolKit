#include <cstdint>
#include <cstdio>

#include <pstk/service_host/TkServiceHost.h>

namespace
{

TkResult NoOpOutput(const TkHostOutputInfo *, void *)
{
    return TK_SUCCESS;
}

} // namespace

int main()
{
    uint32_t version = 0U;
    if (TkServiceHostGetApiVersion(&version) != TK_SUCCESS || version != 1U)
    {
        std::fprintf(stderr, "unexpected Service Host API version: %u\n", static_cast<unsigned int>(version));
        return 1;
    }

    TkServiceHostCreateInfo createInfo{{NoOpOutput, nullptr}};
    TkServiceHost *host = nullptr;
    if (TkServiceHostCreate(&createInfo, &host) != TK_SUCCESS || host == nullptr)
    {
        std::fprintf(stderr, "Service Host create failed\n");
        return 1;
    }

    if (TkServiceHostFinalizeRegistration(host, {nullptr, nullptr}) != TK_SUCCESS)
    {
        std::fprintf(stderr, "empty Service Host finalize failed\n");
        TkServiceHostDestroy(host);
        return 1;
    }

    TkServiceHostDestroy(host);
    TkServiceHostDestroy(nullptr);
    return 0;
}
