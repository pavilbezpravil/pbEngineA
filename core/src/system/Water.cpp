#include "pch.h"
#include "Water.h"

#include "core/CVar.h"
#include "core/Profiler.h"
#include "math/Random.h"
#include "math/Types.h"
#include "rend/CommandList.h"
#include "rend/Renderer.h"
#include "rend/RendRes.h"
#include "rend/Shader.h"
#include "scene/Scene.h"

#include <shared/hlslCppShared.hlsli>

namespace pbe {

   CVarValue<bool> waterDraw{ "render/water/draw", true };

   CVarValue<bool> waterWireframe{ "render/water/wireframe", false };
   CVarValue<bool> waterPixelNormal{ "render/water/pixel normal", false };
   CVarSlider<float> waterWaveScale{ "render/water/wave scale", 1.f, 0.f, 5.f };
   CVarSlider<float> waterTessFactor{ "render/water/tess factor", 64.f, 0.f, 128.f };
   CVarSlider<float> waterPatchSize{ "render/water/patch size", 4.f, 1.f, 32.f };
   CVarSlider<float> waterPatchSizeAAScale{ "render/water/patch size aa scale", 1.f, 0.f, 2.f };
   CVarSlider<int> waterPatchCount{ "render/water/patch count", 256, 1, 512 };
   CVarTrigger waterRecreateWaves{ "render/water/recreate waves" };

   struct WaterWaveDesc {
      float weight = 1;
      int nWaves = 1;

      float lengthMin = 1;
      float lengthMax = 2;

      float amplitudeMin = 1;
      float amplitudeMax = 2;

      float steepness = 1;
      float directionAngleVariance = 90;
   };

   static std::vector<WaterWaveDesc> GenerateWavesDesc() {
      std::vector<WaterWaveDesc> wavesDesc;

      // smallest
      wavesDesc.push_back(
         {
            .weight = 0.8f,
            .nWaves = 16,

            .lengthMin = 1,
            .lengthMax = 3,

            .amplitudeMin = 0.005f,
            .amplitudeMax = 0.015f,

            .directionAngleVariance = 180,
         });

      // small
      wavesDesc.push_back(
         {
            .weight = 0.7f,
            .nWaves = 24,

            .lengthMin = 2,
            .lengthMax = 8,

            .amplitudeMin = 0.015f,
            .amplitudeMax = 0.04f,

            .directionAngleVariance = 180,
         });

      // medium_2
      wavesDesc.push_back(
         {
            .weight = 0.4f,
            .nWaves = 24,

            .lengthMin = 8,
            .lengthMax = 16,

            .amplitudeMin = 0.05f,
            .amplitudeMax = 0.1f,

            .directionAngleVariance = 160,
         });

      // medium
      wavesDesc.push_back(
         {
            .weight = 0.2f,
            .nWaves = 24,

            .lengthMin = 16,
            .lengthMax = 32,

            .amplitudeMin = 0.05f,
            .amplitudeMax = 0.2f,

            .directionAngleVariance = 120,
         });

      // large_2
      wavesDesc.push_back(
         {
            .weight = 0.1f,
            .nWaves = 6,

            .lengthMin = 32,
            .lengthMax = 64,

            .amplitudeMin = 0.3f,
            .amplitudeMax = 0.6f,

            .directionAngleVariance = 90,
         });

      // large
      wavesDesc.push_back(
         {
            .weight = 0.0f,
            .nWaves = 6,

            .lengthMin = 64,
            .lengthMax = 128,

            .amplitudeMin = 1,
            .amplitudeMax = 2,

            .directionAngleVariance = 120,
         });

      // largest
      wavesDesc.push_back(
         {
            .weight = 0.0f,
            .nWaves = 4,

            .lengthMin = 256,
            .lengthMax = 512,

            .amplitudeMin = 4,
            .amplitudeMax = 14,

            .directionAngleVariance = 60,
         });

      return wavesDesc;
   }

   static std::vector<WaterWaveDesc> GenerateWavesDesc2() {
      std::vector<WaterWaveDesc> wavesDesc;

      wavesDesc.push_back(
         {
            .weight = 1.0f,
            .nWaves = 16,

            .lengthMin = 0.2f,
            .lengthMax = 0.5f,

            .amplitudeMin = 0.005f * 0.1f,
            .amplitudeMax = 0.015f * 0.1f,

            .directionAngleVariance = 180,
         });

      wavesDesc.push_back(
         {
            .weight = 1.0f,
            .nWaves = 16,

            .lengthMin = 0.5f,
            .lengthMax = 1.0f,

            .amplitudeMin = 0.005f * 1,
            .amplitudeMax = 0.015f * 1,

            .directionAngleVariance = 180,
         });

      // smallest
      wavesDesc.push_back(
         {
            .weight = 0.8f,
            .nWaves = 16,

            .lengthMin = 1,
            .lengthMax = 3,

            .amplitudeMin = 0.005f,
            .amplitudeMax = 0.015f,

            .directionAngleVariance = 180,
         });
      //
      // // small
      // wavesDesc.push_back(
      //    {
      //       .weight = 0.7f,
      //       .nWaves = 24,
      //
      //       .lengthMin = 2,
      //       .lengthMax = 8,
      //
      //       .amplitudeMin = 0.015f,
      //       .amplitudeMax = 0.04f,
      //
      //       .directionAngleVariance = 180,
      //    });
      //
      // // medium_2
      // wavesDesc.push_back(
      //    {
      //       .weight = 0.4f,
      //       .nWaves = 24,
      //
      //       .lengthMin = 8,
      //       .lengthMax = 16,
      //
      //       .amplitudeMin = 0.05f,
      //       .amplitudeMax = 0.1f,
      //
      //       .directionAngleVariance = 160,
      //    });
      //
      // // medium
      // wavesDesc.push_back(
      //    {
      //       .weight = 0.2f,
      //       .nWaves = 24,
      //
      //       .lengthMin = 16,
      //       .lengthMax = 32,
      //
      //       .amplitudeMin = 0.05f,
      //       .amplitudeMax = 0.2f,
      //
      //       .directionAngleVariance = 120,
      //    });

      return wavesDesc;
   }

   static std::vector<WaveData> GenerateWaves() {
      std::vector<WaterWaveDesc> wavesDesc = GenerateWavesDesc();
      // std::vector<WaterWaveDesc> wavesDesc = GenerateWavesDesc2();

      std::vector<WaveData> waves;

      for (const auto& waveDesc : wavesDesc) {
         if (waveDesc.weight < EPSILON) {
            continue;
         }

         for (int i = 0; i < waveDesc.nWaves; ++i) {
            const float g = 9.8f;

            WaveData wave;

            float wavelength = Random::Float(waveDesc.lengthMin, waveDesc.lengthMax);
            wave.direction = glm::normalize(Random::UniformInCircle()); // todo:
            wave.amplitude = Random::Float(waveDesc.amplitudeMin, waveDesc.amplitudeMax) * waveDesc.weight;
            wave.length = wavelength; // todo:

            wave.magnitude = PI2 / wavelength;
            wave.frequency = sqrt((g * PI2) / wavelength);
            wave.phase = Random::Float(0.f, PI2);
            wave.steepness = waveDesc.steepness;

            waves.emplace_back(wave);
         }
      }

      std::ranges::sort(waves, std::greater{}, &WaveData::amplitude);

      return waves;
   }

   void Water::Render(CommandList& cmd, Scene& scene, CameraContext& cameraContext) {
      if (!waterDraw) {
         return;
      }

      {
         GPU_MARKER("Water Refraction");
         PROFILE_GPU("Water Refraction");

         cmd.CopyResource(*cameraContext.waterRefraction, *cameraContext.colorHDR);
         cmd.CopyResource(*cameraContext.depthWithoutWater, *cameraContext.depth);
      }

      GPU_MARKER("Water");
      PROFILE_GPU("Water");

      // todo: gpu memory leak
      if (!waterWaves || waterRecreateWaves) {
         std::vector<WaveData> waves = GenerateWaves();

         auto bufferDesc = Buffer::Desc::Structured("waver waves", (uint)waves.size(), sizeof(WaveData));
         waterWaves = Buffer::Create(bufferDesc, waves.data());
      }

      cmd.SetRenderTargetsUAV(cameraContext.colorHDR, cameraContext.depth, cameraContext.underCursorBuffer);
      cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);
      cmd.SetBlendState(rendres::blendStateDefaultRGB);

      if (waterWireframe) {
         cmd.SetRasterizerState(rendres::rasterizerStateWireframe);
      }

      auto* context = cmd.pContext;
      context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
      context->IASetInputLayout(nullptr);

      auto waterPass = GetGpuProgram(ProgramDesc::VsHsDsPs("water.hlsl", "waterVS", "waterHS", "waterDS", "waterPS"));
      waterPass->Activate(cmd);

      waterPass->SetSRV(cmd, "gDepth", *cameraContext.depthWithoutWater);
      waterPass->SetSRV(cmd, "gRefraction", *cameraContext.waterRefraction);
      waterPass->SetSRV(cmd, "gWaves", *waterWaves);

      SWaterCB waterCB;

      waterCB.nWaves = waterWaves ? (int)waterWaves->ElementsCount() : 0;

      waterCB.waterTessFactor = waterTessFactor;
      waterCB.waterPatchSize = waterPatchSize;
      waterCB.waterPatchSizeAAScale = waterPatchSizeAAScale;
      waterCB.waterPatchCount = waterPatchCount;
      waterCB.waterPixelNormals = waterPixelNormal;

      waterCB.waterWaveScale = waterWaveScale;

      for (auto [e, trans, water] : scene.View<SceneTransformComponent, WaterComponent>().each()) {
         waterCB.planeHeight = trans.position.y;

         waterCB.fogColor = water.fogColor;
         waterCB.fogUnderwaterLength = water.fogUnderwaterLength;
         waterCB.softZ = water.softZ;

         waterCB.entityID = (uint)e;

         auto dynWaterCB = cmd.AllocDynConstantBuffer(waterCB);
         waterPass->SetCB<SWaterCB>(cmd, "gWaterCB", *dynWaterCB.buffer, dynWaterCB.offset);

         waterPass->DrawInstanced(cmd, 4, waterPatchCount * waterPatchCount);
      }
   }

}
