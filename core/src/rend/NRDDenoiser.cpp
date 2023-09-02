#include "pch.h"
#include "NRDDenoiser.h"

#include "Device.h"
#include "core/Assert.h"
#include "math/Types.h"
#include "Texture2D.h"

#include "NRD.h"
#include "NRI.h"
#include "Extensions/NRIHelper.h"
#include "NRDIntegration.hpp"
#include "Extensions/NRIWrapperD3D11.h"

namespace pbe {

   NrdIntegration NRD = NrdIntegration(2);

   struct NriInterface
      : public nri::CoreInterface
      , public nri::HelperInterface
      , public nri::WrapperD3D11Interface
   {};
   NriInterface NRI;

   nri::Device* nriDevice = nullptr;
   nri::CommandBuffer* nriCommandBuffer = nullptr;
   uint2 nriRenderResolution = { UINT_MAX, UINT_MAX };

   void NRDInit() {
      if (nriDevice) {
         return;
      }

      //=======================================================================================================
      // INITIALIZATION - WRAP NATIVE DEVICE
      //=======================================================================================================

      // Wrap the device
      nri::DeviceCreationD3D11Desc deviceDesc = {};
      deviceDesc.d3d11Device = sDevice->g_pd3dDevice;
      deviceDesc.agsContextAssociatedWithDevice = nullptr;
      // deviceDesc.callbackInterface = nullptr;
      // deviceDesc.memoryAllocatorInterface = nullptr;
      deviceDesc.enableNRIValidation = false;
      deviceDesc.enableAPIValidation = false;

      nri::Result nriResult = nri::CreateDeviceFromD3D11Device(deviceDesc, nriDevice);
      ASSERT(nriResult == nri::Result::SUCCESS);

      // Wrap the command buffer
      nri::CommandBufferD3D11Desc commandBufferDesc = {};
      commandBufferDesc.d3d11DeviceContext = sDevice->g_pd3dDeviceContext;

      // Get core functionality
      nriResult = nri::GetInterface(*nriDevice,
         NRI_INTERFACE(nri::CoreInterface), (nri::CoreInterface*)&NRI);
      ASSERT(nriResult == nri::Result::SUCCESS);

      nriResult = nri::GetInterface(*nriDevice,
         NRI_INTERFACE(nri::HelperInterface), (nri::HelperInterface*)&NRI);
      ASSERT(nriResult == nri::Result::SUCCESS);

      nriResult = nri::GetInterface(*nriDevice,
         NRI_INTERFACE(nri::WrapperD3D11Interface), (nri::WrapperD3D11Interface*)&NRI);
      ASSERT(nriResult == nri::Result::SUCCESS);

      nriResult = NRI.CreateCommandBufferD3D11(*nriDevice, commandBufferDesc, nriCommandBuffer);
      ASSERT(nriResult == nri::Result::SUCCESS);
   }

   constexpr nrd::Identifier NRD_DENOISE_DIFFUSE = 0;
   constexpr nrd::Identifier NRD_DENOISE_SPECULAR = 1;

   void NRDDenoisersInit(uint2 renderResolution) {
      if (nriRenderResolution == renderResolution) {
         return;
      }

      //=======================================================================================================
      // INITIALIZATION - INITIALIZE NRD
      //=======================================================================================================
      const nrd::DenoiserDesc denoiserDescs[] =
      {
         { NRD_DENOISE_DIFFUSE, nrd::Denoiser::REBLUR_DIFFUSE, (uint16_t)renderResolution.x, (uint16_t)renderResolution.y },
         { NRD_DENOISE_SPECULAR, nrd::Denoiser::REBLUR_SPECULAR, (uint16_t)renderResolution.x, (uint16_t)renderResolution.y },
      };

      nrd::InstanceCreationDesc instanceCreationDesc = {};
      instanceCreationDesc.denoisers = denoiserDescs;
      instanceCreationDesc.denoisersNum = _countof(denoiserDescs);

      bool result = NRD.Initialize(instanceCreationDesc, *nriDevice, NRI, NRI);
      ASSERT(result);

      nriRenderResolution = renderResolution;
   }

   void NRDDenoise(const DenoiseCallDesc& desc) {
      //=======================================================================================================
      // INITIALIZATION or RENDER - WRAP NATIVE POINTERS
      //=======================================================================================================

      // Wrap required textures (better do it only once on initialization)
      Texture2D* textures[] = {
         desc.IN_MV, desc.IN_NORMAL_ROUGHNESS, desc.IN_VIEWZ,
         desc.IN_DIFF_RADIANCE_HITDIST, desc.IN_SPEC_RADIANCE_HITDIST,
         desc.OUT_DIFF_RADIANCE_HITDIST, desc.OUT_SPEC_RADIANCE_HITDIST
      };

      constexpr uint32_t N = 7;

      NrdIntegrationTexture integrationTex[N] = {};
      nri::TextureTransitionBarrierDesc entryDescs[N] = {};

      for (uint32_t i = 0; i < N; i++) {
         nri::TextureTransitionBarrierDesc& entryDesc = entryDescs[i];
         Texture2D& tex = *textures[i];

         integrationTex[i].format = nri::ConvertDXGIFormatToNRI(tex.GetDesc().format);
         integrationTex[i].subresourceStates = &entryDesc;

         nri::TextureD3D11Desc textureDesc = {};
         textureDesc.d3d11Resource = tex.GetTexture2D();
         NRI.CreateTextureD3D11(*nriDevice, textureDesc, (nri::Texture*&)entryDesc.texture);

#if 0
         // You need to specify the current state of the resource here, after denoising NRD can modify
         // this state. Application must continue state tracking from this point.
         // Useful information:
         //    SRV = nri::AccessBits::SHADER_RESOURCE, nri::TextureLayout::SHADER_RESOURCE
         //    UAV = nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::TextureLayout::GENERAL
         entryDesc.nextAccess = ConvertResourceStateToAccessBits(tex->GetCurrentState());
         entryDesc.nextLayout = ConvertResourceStateToLayout(tex->GetCurrentState());
#endif
      }

      //=======================================================================================================
      // RENDER - DENOISE
      //=======================================================================================================

      // Set common settings
      //  - for the first time use defaults
      //  - currently NRD supports only the following view space: X - right, Y - top, Z - forward or backward
      nrd::CommonSettings commonSettings = {};
      memcpy(commonSettings.viewToClipMatrix, &desc.mViewToClip, sizeof(desc.mViewToClip));
      memcpy(commonSettings.viewToClipMatrixPrev, &desc.mViewToClipPrev, sizeof(desc.mViewToClipPrev));
      memcpy(commonSettings.worldToViewMatrix, &desc.mWorldToView, sizeof(desc.mWorldToView));
      memcpy(commonSettings.worldToViewMatrixPrev, &desc.mWorldToViewPrev, sizeof(desc.mWorldToViewPrev));
      commonSettings.motionVectorScale[0] = 1.0f / float(desc.textureSize.x);
      commonSettings.motionVectorScale[1] = 1.0f / float(desc.textureSize.y);
      commonSettings.motionVectorScale[2] = 0.0f;

      NRD.SetCommonSettings(commonSettings);

      // Set settings for each method in the NRD instance
      nrd::ReblurSettings settings1 = {};
      NRD.SetDenoiserSettings(NRD_DENOISE_DIFFUSE, &settings1);

      nrd::ReblurSettings settings2 = {};
      NRD.SetDenoiserSettings(NRD_DENOISE_SPECULAR, &settings2);

      // Fill up the user pool
      NrdUserPool userPool = {};
      {
         // Fill only required "in-use" inputs and outputs in appropriate slots using entryDescs & entryFormat,
         // applying remapping if necessary. Unused slots will be {nullptr, nri::Format::UNKNOWN}
         NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_MV, integrationTex[0]);
         NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_NORMAL_ROUGHNESS, integrationTex[1]);
         NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_VIEWZ, integrationTex[2]);
         NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST, integrationTex[3]);
         NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST, integrationTex[4]);
         NrdIntegration_SetResource(userPool, nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST, integrationTex[5]);
         NrdIntegration_SetResource(userPool, nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST, integrationTex[6]);
      };

      // Better use "true" if resources are not changing between frames (i.e. are not suballocated from a heap)
      bool enableDescriptorCaching = true;

      // todo:
      static uint frameIdx = 0;
      frameIdx++;

      NRD.NewFrame(frameIdx);

      // const nri::Result result = NRI.BeginCommandBuffer(nriCommandBuffer, m_DescriptorPool, 0);
      nri::Result result = NRI.BeginCommandBuffer(*nriCommandBuffer, nullptr, 0);
      ASSERT(result == nri::Result::SUCCESS);
      {
         // todo:
         // const nrd::Identifier denoisers[] = { NRD_DENOISE_DIFFUSE, NRD_DENOISE_SPECULAR };
         const nrd::Identifier denoisers[] = { NRD_DENOISE_DIFFUSE };
         NRD.Denoise(denoisers, _countof(denoisers), *nriCommandBuffer, userPool, enableDescriptorCaching);
      }
      result = NRI.EndCommandBuffer(*nriCommandBuffer);
      ASSERT(result == nri::Result::SUCCESS);

      nri::QueueSubmitDesc queueSubmitDesc = {};
      queueSubmitDesc.physicalDeviceIndex = 0;
      queueSubmitDesc.commandBufferNum = 1;
      queueSubmitDesc.commandBuffers = &nriCommandBuffer;

      nri::CommandQueue* nriQueue = nullptr;
      NRI.GetCommandQueue(*nriDevice, nri::CommandQueueType::GRAPHICS, nriQueue);

      NRI.QueueSubmit(*nriQueue, queueSubmitDesc);

      // IMPORTANT: NRD integration binds own descriptor pool, don't forget to re-bind back your pool (heap)

      // Better do it only once on shutdown
      for (uint32_t i = 0; i < N; i++) {
         nri::Texture* tex = const_cast<nri::Texture*>(entryDescs[i].texture);
         NRI.DestroyTexture(*tex);
      }
   }

   void NRDTerm() {
      if (!nriDevice) {
         return;
      }

      //=======================================================================================================
      // SHUTDOWN or RENDER - CLEANUP
      //=======================================================================================================

      NRI.DestroyCommandBuffer(*nriCommandBuffer);

      //=======================================================================================================
      // SHUTDOWN - DESTROY
      //=======================================================================================================

      // Release wrapped device
      nri::DestroyDevice(*nriDevice);

      nriDevice = nullptr;
      nriCommandBuffer = nullptr;
      nriRenderResolution = { UINT_MAX, UINT_MAX };

      // Also NRD needs to be recreated on "resize"
      NRD.Destroy();
   }

   void NRDResize(uint2 renderResolution) {
      if (nriRenderResolution != renderResolution || !nriDevice) {
         NRDTerm();
         NRDInit();
      }

      NRDDenoisersInit(renderResolution);
   }

}
