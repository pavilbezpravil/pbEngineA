#include "NRDIntegration.h"
#include "Buffer.h"
#include "Device.h"
#include "CommandList.h"
#include "Texture2D.h"
// #include "core/Profiler.h"
#include "d3d11.h"


constexpr std::array<DXGI_FORMAT, (size_t)nrd::Format::MAX_NUM> g_NRD_NrdToNriFormat2 =
{
    DXGI_FORMAT_R8_UNORM,
    DXGI_FORMAT_R8_SNORM,
    DXGI_FORMAT_R8_UINT,
    DXGI_FORMAT_R8_SINT,
    DXGI_FORMAT_R8G8_UNORM,
    DXGI_FORMAT_R8G8_SNORM,
    DXGI_FORMAT_R8G8_UINT,
    DXGI_FORMAT_R8G8_SINT,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R8G8B8A8_SNORM,
    DXGI_FORMAT_R8G8B8A8_UINT,
    DXGI_FORMAT_R8G8B8A8_SINT,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    DXGI_FORMAT_R16_UNORM,
    DXGI_FORMAT_R16_SNORM,
    DXGI_FORMAT_R16_UINT,
    DXGI_FORMAT_R16_SINT,
    DXGI_FORMAT_R16_FLOAT,
    DXGI_FORMAT_R16G16_UNORM,
    DXGI_FORMAT_R16G16_SNORM,
    DXGI_FORMAT_R16G16_UINT,
    DXGI_FORMAT_R16G16_SINT,
    DXGI_FORMAT_R16G16_FLOAT,
    DXGI_FORMAT_R16G16B16A16_UNORM,
    DXGI_FORMAT_R16G16B16A16_SNORM,
    DXGI_FORMAT_R16G16B16A16_UINT,
    DXGI_FORMAT_R16G16B16A16_SINT,
    DXGI_FORMAT_R16G16B16A16_FLOAT,
    DXGI_FORMAT_R32_UINT,
    DXGI_FORMAT_R32_SINT,
    DXGI_FORMAT_R32_FLOAT,
    DXGI_FORMAT_R32G32_UINT,
    DXGI_FORMAT_R32G32_SINT,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_R32G32B32_UINT,
    DXGI_FORMAT_R32G32B32_SINT,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_UINT,
    DXGI_FORMAT_R32G32B32A32_SINT,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_R10G10B10A2_UNORM,
    DXGI_FORMAT_R10G10B10A2_UINT,
    DXGI_FORMAT_R11G11B10_FLOAT,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
};

static inline DXGI_FORMAT NRD_GetNriFormat(nrd::Format format) {
    return g_NRD_NrdToNriFormat2[(uint32_t)format];
}

bool NrdIntegration::Initialize(const nrd::InstanceCreationDesc& instanceCreationDesc) {
    const nrd::LibraryDesc& libraryDesc = nrd::GetLibraryDesc();
    if (libraryDesc.versionMajor != NRD_VERSION_MAJOR || libraryDesc.versionMinor != NRD_VERSION_MINOR) {
        ASSERT_MESSAGE(false, "NRD version mismatch detected!");
        return false;
    }

    if (nrd::CreateInstance(instanceCreationDesc, m_Instance) != nrd::Result::SUCCESS)
        return false;

    CreatePipelines();
    CreateResources();

    return true;
}

void NrdIntegration::CreatePipelines() {
   const nrd::InstanceDesc& instanceDesc = nrd::GetInstanceDesc(*m_Instance);

    // Assuming that the device is in IDLE state
    m_Pipelines2.clear();

    // Pipelines
   for (uint32_t i = 0; i < instanceDesc.pipelinesNum; i++) {
      const nrd::PipelineDesc& nrdPipelineDesc = instanceDesc.pipelines[i];

      const nrd::ComputeShaderDesc& nrdComputeShader = nrdPipelineDesc.computeShaderDXBC;

      m_Pipelines2.push_back({});
      pbe::sDevice->g_pd3dDevice->CreateComputeShader(
         nrdComputeShader.bytecode, nrdComputeShader.size,
         nullptr, &m_Pipelines2.back());
      SetDbgName(m_Pipelines2.back().Get(), nrdPipelineDesc.shaderFileName);
   }
}

void NrdIntegration::CreateResources() {
   auto device = pbe::sDevice->g_pd3dDevice;

    const nrd::InstanceDesc& instanceDesc = nrd::GetInstanceDesc(*m_Instance);
    const uint32_t poolSize = instanceDesc.permanentPoolSize + instanceDesc.transientPoolSize;

    m_TexturePool2.resize(poolSize);

    for (uint32_t i = 0; i < poolSize; i++) {
        const nrd::TextureDesc& nrdTextureDesc = (i < instanceDesc.permanentPoolSize) ? instanceDesc.permanentPool[i] : instanceDesc.transientPool[i - instanceDesc.permanentPoolSize];

        ASSERT(nrdTextureDesc.mipNum == 1);

        Texture2D::Desc desc = {
            .size = { nrdTextureDesc.width, nrdTextureDesc.height },
            .mips = nrdTextureDesc.mipNum,
            .format = NRD_GetNriFormat(nrdTextureDesc.format),
            .bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
        };

        m_TexturePool2[i] = Texture2D::Create(desc);
    }

    for (uint32_t i = 0; i < instanceDesc.samplersNum; i++) {
        nrd::Sampler nrdSampler = instanceDesc.samplers[i];

        D3D11_SAMPLER_DESC samplerDesc2 = {};
        samplerDesc2.MaxLOD = 16;

        if (nrdSampler == nrd::Sampler::NEAREST_CLAMP || nrdSampler == nrd::Sampler::LINEAR_CLAMP) {
           samplerDesc2.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
           samplerDesc2.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
           samplerDesc2.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        } else {
           samplerDesc2.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
           samplerDesc2.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
           samplerDesc2.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
        }

        if (nrdSampler == nrd::Sampler::NEAREST_CLAMP || nrdSampler == nrd::Sampler::NEAREST_MIRRORED_REPEAT) {
            samplerDesc2.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        } else {
            samplerDesc2.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        }

        samplerDesc2.ComparisonFunc = D3D11_COMPARISON_NEVER;

        m_Samplers2.push_back({});
        device->CreateSamplerState(&samplerDesc2, &m_Samplers2.back());
    }
}

bool NrdIntegration::SetCommonSettings(const nrd::CommonSettings& commonSettings) {
    nrd::Result result = nrd::SetCommonSettings(*m_Instance, commonSettings);
    ASSERT(result == nrd::Result::SUCCESS);

    return result == nrd::Result::SUCCESS;
}

bool NrdIntegration::SetDenoiserSettings(nrd::Identifier denoiser, const void* denoiserSettings) {
    nrd::Result result = nrd::SetDenoiserSettings(*m_Instance, denoiser, denoiserSettings);
    ASSERT(result == nrd::Result::SUCCESS);

    return result == nrd::Result::SUCCESS;
}

void NrdIntegration::Denoise(const nrd::Identifier* denoisers, uint32_t denoisersNum, CommandList& cmd, const NrdUserPool& userPool) {
    const nrd::DispatchDesc* dispatchDescs = nullptr;
    uint32_t dispatchDescsNum = 0;
    nrd::GetComputeDispatches(*m_Instance, denoisers, denoisersNum, dispatchDescs, dispatchDescsNum);

    for (uint32_t i = 0; i < dispatchDescsNum; i++) {
        const nrd::DispatchDesc& dispatchDesc = dispatchDescs[i];

        GPU_MARKER(dispatchDesc.name);
        // PROFILE_GPU(dispatchDesc.name);

        Dispatch(cmd, dispatchDesc, userPool);
    }
}

void NrdIntegration::Dispatch(CommandList& cmd, const nrd::DispatchDesc& dispatchDesc, const NrdUserPool& userPool) {
   auto pContext = cmd.pContext;

    const nrd::InstanceDesc& instanceDesc = nrd::GetInstanceDesc(*m_Instance);
    const nrd::PipelineDesc& pipelineDesc = instanceDesc.pipelines[dispatchDesc.pipelineIndex];

   struct Bindings {
      bool isUav;
      uint slot;
   };

   static std::vector<Bindings> binds;

    uint nSrv = 0;
    uint nUav = 0;

    uint32_t n = 0;
    for (uint32_t i = 0; i < pipelineDesc.resourceRangesNum; i++) {
        const nrd::ResourceRangeDesc& resourceRange = pipelineDesc.resourceRanges[i];
        const bool isUav = resourceRange.descriptorType == nrd::DescriptorType::STORAGE_TEXTURE;

        for (uint32_t j = 0; j < resourceRange.descriptorsNum; j++) {
           const nrd::ResourceDesc& nrdResource = dispatchDesc.resources[n];

           ASSERT(nrdResource.mipNum == 1);
           ASSERT(nrdResource.mipOffset == 0);

           Texture2D* nrdTex = nullptr;
           if (nrdResource.type == nrd::ResourceType::TRANSIENT_POOL) {
               nrdTex = m_TexturePool2[nrdResource.indexInPool + instanceDesc.permanentPoolSize];
            } else if (nrdResource.type == nrd::ResourceType::PERMANENT_POOL) {
               nrdTex = m_TexturePool2[nrdResource.indexInPool];
            } else {
               nrdTex = userPool[(uint32_t)nrdResource.type];
               ASSERT_MESSAGE(nrdTex, "'userPool' entry can't be NULL if it's in use!");
            }

           // todo:
            binds.emplace_back(Bindings{ isUav, resourceRange.baseRegisterIndex + (isUav ? nUav : nSrv) });

           if (!isUav) {
              auto srv = nrdTex->srv;
              pContext->CSSetShaderResources(resourceRange.baseRegisterIndex + nSrv++, 1, srv.GetAddressOf());
           } else {
              auto uav = nrdTex->uav;
              pContext->CSSetUnorderedAccessViews(resourceRange.baseRegisterIndex + nUav++, 1, uav.GetAddressOf(), nullptr);
           }

            n++;
        }
    }

    ASSERT(instanceDesc.constantBufferSpaceIndex == 0);
    ASSERT(instanceDesc.samplersSpaceIndex == 0);
    ASSERT(instanceDesc.resourcesSpaceIndex == 0);
    ASSERT(instanceDesc.constantBufferSpaceIndex == 0);

    // Updating constants
    if (dispatchDesc.constantBufferDataSize) {
       ASSERT(pipelineDesc.hasConstantData);
       // todo: I think all cb in 0 slot
       cmd.AllocAndSetCB({ 0 }, dispatchDesc.constantBufferData, dispatchDesc.constantBufferDataSize);
    } else {
       ASSERT(!pipelineDesc.hasConstantData);
    }

    cmd.pContext->CSSetSamplers(instanceDesc.samplersBaseRegisterIndex, instanceDesc.samplersNum, m_Samplers2[0].GetAddressOf());

    auto computeShader = m_Pipelines2[dispatchDesc.pipelineIndex];
    pContext->CSSetShader(computeShader.Get(), nullptr, 0);

    cmd.Dispatch2D({ dispatchDesc.gridWidth, dispatchDesc.gridHeight });

    // unbind
    for (auto bind : binds) {
       if (!bind.isUav) {
          ID3D11ShaderResourceView* srvs[] = { nullptr };
          pContext->CSSetShaderResources(bind.slot, _countof(srvs), srvs);
       } else {
          ID3D11UnorderedAccessView* uavs[] = { nullptr};
          pContext->CSSetUnorderedAccessViews(bind.slot, _countof(uavs), uavs, nullptr);
       }
    }
    binds.clear();
}

void NrdIntegration::Destroy() {
   m_TexturePool2.clear();
   m_Pipelines2.clear();
   m_Samplers2.clear();

    nrd::DestroyInstance(*m_Instance);
    m_Instance = nullptr;
}
