/*
Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

// IMPORTANT: these files must be included beforehand:
//    NRD.h
//    NRIDescs.h
//    Extensions/NRIHelper.h
//    Extensions/NRIWrapperD3D11.h
//    Extensions/NRIWrapperD3D12.h
//    Extensions/NRIWrapperVK.h

#include <array>
#include <vector>
#include <map>


namespace pbe {
   class CommandList;
   class Buffer;
}

using namespace pbe;

#define NRD_INTEGRATION_MAJOR 1
#define NRD_INTEGRATION_MINOR 6
#define NRD_INTEGRATION_DATE "30 April 2023"
#define NRD_INTEGRATION 1

#define NRD_INTEGRATION_DEBUG_LOGGING 0

#ifndef NRD_INTEGRATION_ASSERT
    #include <assert.h>
    #define NRD_INTEGRATION_ASSERT(expr, msg) (assert(expr && msg))
#endif

// User inputs / outputs are not mipmapped, thus only 1 entry is needed.
// "TextureTransitionBarrierDesc::texture" is used to store the resource.
// "TextureTransitionBarrierDesc::next..." are used to represent the current state of the subresource.
struct NrdIntegrationTexture
{
    nri::TextureTransitionBarrierDesc* subresourceStates;
    nri::Format format;
};

typedef std::array<Texture2D*, (size_t)nrd::ResourceType::MAX_NUM - 2> NrdUserPool;

// User pool must contain valid entries only for resources, which are required for requested denoisers, but
// the entire pool must be zero-ed during initialization
inline void NrdIntegration_SetResource(NrdUserPool& pool, nrd::ResourceType slot, Texture2D* texture) {
   ASSERT(texture);
    pool[(size_t)slot] = texture;
}

class NrdIntegration
{
public:
    // The application must provide number of buffered frames, it's needed to guarantee that
    // constant data and descriptor sets are not overwritten while being executed on the GPU.
    // Usually it's 2-3 frames.
    NrdIntegration(uint32_t bufferedFramesNum, const char* persistentName = "") :
        m_Name(persistentName) {}

    ~NrdIntegration() { }

    // There is no "Resize" functionality, because NRD full recreation costs nothing.
    // The main cost comes from render targets resizing, which needs to be done in any case
    // (call Destroy beforehand)
    bool Initialize(const nrd::InstanceCreationDesc& instanceCreationDesc, nri::Device& nriDevice,
        const nri::CoreInterface& nriCore, const nri::HelperInterface& nriHelper);

    // Must be called once on a frame start
    void NewFrame(uint32_t frameIndex);

    // Explcitly calls eponymous NRD API functions
    bool SetCommonSettings(const nrd::CommonSettings& commonSettings);
    bool SetDenoiserSettings(nrd::Identifier denoiser, const void* denoiserSettings);

    // Better use "enableDescriptorCaching = true" if resources are not changing between frames
    // (i.e. not suballocated from a heap)
    void Denoise(const nrd::Identifier* denoisers, uint32_t denoisersNum,
        nri::CommandBuffer& commandBuffer, CommandList& cmd, const NrdUserPool& userPool,
        bool enableDescriptorCaching
    );

    // This function assumes that the device is in the IDLE state, i.e. there is no work in flight
    void Destroy();

    // Should not be called explicitly, unless you want to reload pipelines
    void CreatePipelines();

    bool Inited() const { return m_Instance != nullptr; }

private:
    NrdIntegration(const NrdIntegration&) = delete;

    void CreateResources();
    void Dispatch(CommandList& cmd, const nrd::DispatchDesc& dispatchDesc, const NrdUserPool& userPool);

private:
    std::vector<Ref<Texture2D>> m_TexturePool2;
    std::vector<ComPtr<ID3D11ComputeShader>> m_Pipelines2;
    std::vector<ComPtr<ID3D11SamplerState>> m_Samplers2;

    nrd::Instance* m_Instance = nullptr;
    const char* m_Name = nullptr;
};

#define NRD_INTEGRATION_ABORT_ON_FAILURE(result) if ((result) != nri::Result::SUCCESS) NRD_INTEGRATION_ASSERT(false, "Abort on failure!")
