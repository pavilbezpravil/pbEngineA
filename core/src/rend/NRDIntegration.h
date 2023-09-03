#pragma once

// IMPORTANT: these files must be included beforehand:
//    NRD.h
//    NRIDescs.h

#include <array>
#include <vector>
#include <map>


namespace pbe {
   class CommandList;
   class Buffer;
}

using namespace pbe;

typedef std::array<Texture2D*, (size_t)nrd::ResourceType::MAX_NUM - 2> NrdUserPool;

// User pool must contain valid entries only for resources, which are required for requested denoisers, but
// the entire pool must be zero-ed during initialization
inline void NrdIntegration_SetResource(NrdUserPool& pool, nrd::ResourceType slot, Texture2D* texture) {
   ASSERT(texture);
    pool[(size_t)slot] = texture;
}

class NrdIntegration {
public:
    // There is no "Resize" functionality, because NRD full recreation costs nothing.
    // The main cost comes from render targets resizing, which needs to be done in any case
    // (call Destroy beforehand)
    bool Initialize(const nrd::InstanceCreationDesc& instanceCreationDesc);

    // Explcitly calls eponymous NRD API functions
    bool SetCommonSettings(const nrd::CommonSettings& commonSettings);
    bool SetDenoiserSettings(nrd::Identifier denoiser, const void* denoiserSettings);

    void Denoise(const nrd::Identifier* denoisers, uint32_t denoisersNum,
       CommandList& cmd, const NrdUserPool& userPool);

    // This function assumes that the device is in the IDLE state, i.e. there is no work in flight
    void Destroy();

    // Should not be called explicitly, unless you want to reload pipelines
    void CreatePipelines();

    bool Inited() const { return m_Instance != nullptr; }

private:
    void CreateResources();
    void Dispatch(CommandList& cmd, const nrd::DispatchDesc& dispatchDesc, const NrdUserPool& userPool);

    std::vector<Ref<Texture2D>> m_TexturePool2;
    std::vector<ComPtr<ID3D11ComputeShader>> m_Pipelines2;
    std::vector<ComPtr<ID3D11SamplerState>> m_Samplers2;

    nrd::Instance* m_Instance = nullptr;
};
