#include "pch.h"
#include "Renderer.h"

#include "DbgRend.h"
#include "RendRes.h"
#include "RTRenderer.h"
#include "core/CVar.h"
#include "core/Profiler.h"
#include "math/Random.h"
#include "math/Shape.h"

#include "shared/hlslCppShared.hlsli"


namespace pbe {

   CVarValue<bool> dbgRenderEnable{ "render/debug render", true };
   CVarValue<bool> instancedDraw{ "render/instanced draw", true };
   CVarValue<bool> indirectDraw{ "render/indirect draw", true };
   CVarValue<bool> depthDownsampleEnable{ "render/depth downsample enable", true };
   CVarValue<bool> rayTracingSceneRender{ "render/ray tracing scene render", false };
   CVarValue<bool> animationTimeUpdate{ "render/animation time update", true };

   CVarValue<bool> applyFog{ "render/fog/enable", true };
   CVarSlider<int> fogNSteps{ "render/fog/nSteps", 0, 0, 128 };

   CVarSlider<float> tonemapExposition{ "render/tonemap/exposion", 1.f, 0.f, 1.f };

   CVarValue<bool> waterDraw{ "render/water/draw", true };
   CVarValue<bool> waterWireframe{ "render/water/wireframe", false };
   CVarValue<bool> waterPixelNormal{ "render/water/pixel normal", false };
   CVarSlider<float> waterWaveScale{ "render/water/wave scale", 1.f, 0.f, 5.f };
   CVarSlider<float> waterTessFactor{ "render/water/tess factor", 64.f, 0.f, 128.f };
   CVarSlider<float> waterPatchSize{ "render/water/patch size", 4.f, 1.f, 32.f };
   CVarSlider<float> waterPatchSizeAAScale{ "render/water/patch size aa scale", 1.f, 0.f, 2.f };
   CVarSlider<int> waterPatchCount{ "render/water/patch count", 256, 1, 512 };
   CVarTrigger waterRecreateWaves{ "render/water/recreate waves" };

   static mat4 NDCToTexSpaceMat4() {
      mat4 scale = glm::scale(mat4{1.f}, {0.5f, -0.5f, 1.f});
      mat4 translate = glm::translate(mat4{1.f}, {0.5f, 0.5f, 0.f});

      return translate * scale;
   }

   void RenderCamera::FillSCameraCB(SCameraCB& cameraCB) const {
      cameraCB.view = glm::transpose(view);
      cameraCB.projection = glm::transpose(projection);
      cameraCB.viewProjection = glm::transpose(GetViewProjection());
      cameraCB.invViewProjection = glm::inverse(cameraCB.viewProjection);
      cameraCB.position = position;

      cameraCB.zNear = zNear;
      cameraCB.zFar = zFar;
   }

   void Renderer::Init() {
      rtRenderer.reset(new RTRenderer());
      rtRenderer->Init();

      auto programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "ps_main");
      programDesc.vs.defines.AddDefine("ZPASS");
      programDesc.ps.defines.AddDefine("ZPASS");
      baseZPass = GpuProgram::Create(programDesc);

      programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main");
      programDesc.vs.defines.AddDefine("ZPASS");
      shadowMapPass = GpuProgram::Create(programDesc);

      programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "ps_main");
      baseColorPass = GpuProgram::Create(programDesc);

      const uint underCursorSize = 1;
      uint underCursorInitialData[underCursorSize] = { (uint)-1 };
      auto bufferDesc = Buffer::Desc::Structured("under cursor buffer",
         underCursorSize, sizeof(uint), D3D11_BIND_UNORDERED_ACCESS);
      underCursorBuffer = Buffer::Create(bufferDesc, underCursorInitialData);

      mesh = Mesh::Create(MeshGeomCube());
   }

   void Renderer::UpdateInstanceBuffer(CommandList& cmd, const std::vector<RenderObject>& renderObjs) {
      if (!instanceBuffer || instanceBuffer->ElementsCount() < renderObjs.size()) {
         auto bufferDesc = Buffer::Desc::Structured("instance buffer", (uint)renderObjs.size(), sizeof(SInstance));
         instanceBuffer = Buffer::Create(bufferDesc);
      }

      std::vector<SInstance> instances;
      instances.reserve(renderObjs.size());
      for (auto& [trans, material] : renderObjs) {
         mat4 transform = glm::transpose(trans.GetMatrix());

         SMaterial m;
         m.albedo = material.albedo;
         m.roughness = material.roughness;
         m.metallic = material.metallic;

         SInstance instance;
         instance.material = m;
         instance.transform = transform;
         instance.entityID = (uint)trans.entity.GetID();

         instances.emplace_back(instance);
      }

      cmd.UpdateSubresource(*instanceBuffer, instances.data(), 0, instances.size() * sizeof(SInstance));
   }

   void Renderer::RenderDataPrepare(CommandList& cmd, Scene& scene) {
      opaqueObjs.clear();
      transparentObjs.clear();

      for (auto [e, sceneTrans, material] :
         scene.GetEntitiesWith<SceneTransformComponent, SimpleMaterialComponent>().each()) {

         // DrawDesc desc;
         // desc.entityID = (uint)e;
         // desc.transform = sceneTrans.GetMatrix();
         // desc.material = { material, true };
         //
         // drawDescs.push_back(desc);

         if (material.opaque) {
            opaqueObjs.emplace_back(sceneTrans, material);
         } else {
            transparentObjs.emplace_back(sceneTrans, material);
         }
      }

      if (!instanceBuffer || instanceBuffer->ElementsCount() < scene.EntitiesCount()) {
         auto bufferDesc = Buffer::Desc::Structured("instance buffer", scene.EntitiesCount(), sizeof(SInstance));
         instanceBuffer = Buffer::Create(bufferDesc);
      }

      {
         auto nLights = (uint)scene.GetEntitiesWith<LightComponent>().size();
         if (!lightBuffer || lightBuffer->ElementsCount() < nLights) {
            auto bufferDesc = Buffer::Desc::Structured("light buffer", nLights, sizeof(SLight));
            lightBuffer = Buffer::Create(bufferDesc);
         }

         std::vector<SLight> lights;
         lights.reserve(nLights);

         for (auto [e, trans, light] : scene.GetEntitiesWith<SceneTransformComponent, LightComponent>().each()) {
            SLight l;
            l.position = trans.position;
            l.color = light.color;
            l.radius = light.radius;
            l.type = SLIGHT_TYPE_POINT;

            lights.emplace_back(l);
         }

         cmd.UpdateSubresource(*lightBuffer, lights.data(), 0, lights.size() * sizeof(SLight));
      }

      if (!ssaoRandomDirs) {
         int nRandomDirs = 32;
         auto bufferDesc = Buffer::Desc::Structured("ssao random dirs", nRandomDirs, sizeof(vec3));
         ssaoRandomDirs = Buffer::Create(bufferDesc);

         std::vector<vec3> dirs;
         dirs.reserve(nRandomDirs);

         for (int i = 0; i < nRandomDirs; ++i) {
            vec3 test;
            do {
               test = Random::Uniform(vec3{ -1 }, vec3{ 1 });
            } while (glm::length(test) > 1);

            dirs.emplace_back(test);
         }

         cmd.UpdateSubresource(*ssaoRandomDirs, dirs.data(), 0, dirs.size() * sizeof(vec3));
      }
   }

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

            float wavelength = Random::Uniform(waveDesc.lengthMin, waveDesc.lengthMax);
            wave.direction = glm::normalize(Random::UniformInCircle()); // todo:
            wave.amplitude = Random::Uniform(waveDesc.amplitudeMin, waveDesc.amplitudeMax) * waveDesc.weight;
            wave.length = wavelength; // todo:

            wave.magnitude = PI2 / wavelength;
            wave.frequency = sqrt((g * PI2) / wavelength);
            wave.phase = Random::Uniform(0.f, PI2);
            wave.steepness = waveDesc.steepness;

            waves.emplace_back(wave);
         }
      }

      std::ranges::sort(waves, std::greater{}, &WaveData::amplitude);

      return waves;
   }

   void Renderer::RenderScene(CommandList& cmd, Scene& scene, const RenderCamera& camera,
      CameraContext& cameraContext) {
      if (!baseColorPass->Valid() || !baseZPass->Valid()) {
         return;
      }

      GPU_MARKER("Render Scene");
      PROFILE_GPU("Render Scene");

      RenderDataPrepare(cmd, scene);

      cmd.ClearRenderTarget(*cameraContext.colorLDR, vec4{0, 0, 0, 1});
      cmd.ClearRenderTarget(*cameraContext.colorHDR, vec4{0, 0, 0, 1});
      cmd.ClearRenderTarget(*cameraContext.normal, vec4{0, 0, 0, 0});
      cmd.ClearDepthTarget(*cameraContext.depth, 1);

      cmd.SetRasterizerState(rendres::rasterizerState);

      uint nDecals = 0;
      if (cfg.decals) {
         std::vector<SDecal> decals;

         SimpleMaterialComponent decalDefault{}; // todo:
         for (auto [e, trans, decal] : scene.GetEntitiesWith<SceneTransformComponent, DecalComponent>().each()) {
            decalObjs.emplace_back(trans, decalDefault);

            vec3 size = trans.scale * 0.5f;

            mat4 view = glm::lookAt(trans.position, trans.position + trans.Forward(), trans.Up());
            mat4 projection = glm::ortho(-size.x, size.x, -size.y, size.y, -size.z, size.z);
            mat4 viewProjection = glm::transpose(projection * view);
            decals.emplace_back(viewProjection, decal.albedo, decal.metallic, decal.roughness);
         }

         nDecals = (uint)decals.size();

         if (!decalBuffer || decalBuffer->ElementsCount() < nDecals) {
            auto bufferDesc = Buffer::Desc::Structured("Decals", nDecals, sizeof(SDecal));
            decalBuffer = Buffer::Create(bufferDesc);
         }

         cmd.UpdateSubresource(*decalBuffer, decals.data(), 0, nDecals * sizeof(SDecal));
      }

      RenderCamera shadowCamera;

      SSceneCB sceneCB;

      static int iFrame = 0; // todo:
      ++iFrame;
      sceneCB.iFrame = iFrame;
      sceneCB.deltaTime = 1 / 60.0f; // todo:
      sceneCB.time = sceneCB.deltaTime * sceneCB.iFrame;

      sceneCB.animationTime = std::invoke([&] {
         static float animTime = 0;
         if (animationTimeUpdate) {
            animTime += sceneCB.deltaTime;
         }
         return animTime;
      });

      sceneCB.nWaves = waterWaves ? (int)waterWaves->ElementsCount() : 0;
      sceneCB.fogNSteps = fogNSteps;

      sceneCB.nLights = (int)scene.GetEntitiesWith<LightComponent>().size();
      sceneCB.nDecals = (int)nDecals;

      sceneCB.waterTessFactor = waterTessFactor;
      sceneCB.waterPatchSize = waterPatchSize;
      sceneCB.waterPatchSizeAAScale = waterPatchSizeAAScale;
      sceneCB.waterPatchCount = waterPatchCount;
      sceneCB.waterPixelNormals = waterPixelNormal;

      sceneCB.waterWaveScale = waterWaveScale;

      sceneCB.exposition = tonemapExposition;

      sceneCB.directLight.color = {};
      sceneCB.directLight.direction = vec3{1, 0, 0};
      sceneCB.directLight.type = SLIGHT_TYPE_DIRECT;

      auto directLightsView = scene.GetEntitiesWith<SceneTransformComponent, DirectLightComponent>();
      bool hasDirectLight = directLightsView.size_hint() > 0;

      if (hasDirectLight) {
         auto [_, trans, directLight] = *directLightsView.each().begin();

         sceneCB.directLight.color = directLight.color;
         sceneCB.directLight.direction = trans.Forward();

         const float halfSize = 25;
         const float halfDepth = 50;

         auto shadowSpace = glm::lookAt({}, sceneCB.directLight.direction, trans.Up());
         vec3 posShadowSpace = shadowSpace * vec4(camera.position, 1);

         vec2 shadowMapTexels = cameraContext.shadowMap->GetDesc().size;
         vec3 shadowTexelSize = vec3{ 2.f * halfSize / shadowMapTexels, 2.f * halfDepth / (1 << 16) };
         vec3 snappedPosShadowSpace = glm::ceil(posShadowSpace / shadowTexelSize) * shadowTexelSize;

         vec3 snappedPosW = glm::inverse(shadowSpace) * vec4(snappedPosShadowSpace, 1);

         shadowCamera.position = snappedPosW;

         shadowCamera.projection = glm::ortho<float>(-halfSize, halfSize, -halfSize, halfSize, -halfDepth, halfDepth);
         shadowCamera.view = glm::lookAt(shadowCamera.position, shadowCamera.position + sceneCB.directLight.direction, trans.Up());
         sceneCB.toShadowSpace = glm::transpose(NDCToTexSpaceMat4() * shadowCamera.GetViewProjection());
      }

      cmd.AllocAndSetCommonCB(CB_SLOT_SCENE, sceneCB);

      if (cfg.opaqueSorting) {
         // todo: slow. I assumed
         std::ranges::sort(opaqueObjs, [&](RenderObject& a, RenderObject& b) {
            float az = glm::dot(camera.Forward(), a.trans.position);
            float bz = glm::dot(camera.Forward(), b.trans.position);
            return az < bz;
         });
      }
      UpdateInstanceBuffer(cmd, opaqueObjs);

      cmd.pContext->ClearUnorderedAccessViewFloat(cameraContext.ssao->uav.Get(), &vec4_One.x);

      uint4 clearValue = { -1, -1, -1, -1 };
      cmd.pContext->ClearUnorderedAccessViewUint(underCursorBuffer->uav.Get(), &clearValue.x);

      SCameraCB cameraCB;
      camera.FillSCameraCB(cameraCB);
      cameraCB.rtSize = cameraContext.colorHDR->GetDesc().size;
      cmd.AllocAndSetCommonCB(CB_SLOT_CAMERA, cameraCB);

      SEditorCB editorCB;
      editorCB.cursorPixelIdx = cameraContext.cursorPixelIdx;
      cmd.AllocAndSetCommonCB(CB_SLOT_EDITOR, editorCB);

      // todo:
      auto ResetCS_SRV_UAV = [&] {
         ID3D11ShaderResourceView* viewsSRV[] = { nullptr, nullptr };
         cmd.pContext->CSSetShaderResources(0, _countof(viewsSRV), viewsSRV);

         ID3D11UnorderedAccessView* viewsUAV[] = { nullptr, nullptr };
         cmd.pContext->CSSetUnorderedAccessViews(0, _countof(viewsUAV), viewsUAV, nullptr);
      };

      if (rayTracingSceneRender) {
         rtRenderer->RenderScene(cmd, scene, camera, cameraContext);
         ResetCS_SRV_UAV();
      } else {
         if (cfg.useShadowPass && hasDirectLight) {
            GPU_MARKER("Shadow Map");
            PROFILE_GPU("Shadow Map");

            cmd.ClearDepthTarget(*cameraContext.shadowMap, 1);

            cmd.SetRenderTargets(nullptr, cameraContext.shadowMap);
            cmd.SetViewport({}, cameraContext.shadowMap->GetDesc().size);
            cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);
            cmd.SetBlendState(rendres::blendStateDefaultRGBA);

            cmd.pContext->PSSetSamplers(0, 1, &rendres::samplerStateWrapPoint); // todo:

            SCameraCB shadowCameraCB;
            shadowCamera.FillSCameraCB(shadowCameraCB);
            // shadowCameraCB.rtSize = cameraContext.colorHDR->GetDesc().size;

            cmd.AllocAndSetCommonCB(CB_SLOT_CAMERA, shadowCameraCB);
            RenderSceneAllObjects(cmd, opaqueObjs, *shadowMapPass, cameraContext);
         }

         cmd.AllocAndSetCommonCB(CB_SLOT_CAMERA, cameraCB); // todo: set twice

         cmd.SetViewport({}, cameraContext.colorHDR->GetDesc().size); /// todo:

         if (cfg.useZPass) {
            {
               GPU_MARKER("ZPass");
               PROFILE_GPU("ZPass");

               cmd.SetRenderTargets(cameraContext.normal, cameraContext.depth);
               cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);
               cmd.SetBlendState(rendres::blendStateDefaultRGBA);

               RenderSceneAllObjects(cmd, opaqueObjs, *baseZPass, cameraContext);
            }

            if (cfg.ssao) {
               GPU_MARKER("SSAO");
               PROFILE_GPU("SSAO");

               cmd.SetRenderTargets();

               auto ssaoPass = GetGpuProgram(ProgramDesc::Cs("ssao.cs", "main"));

               ssaoPass->Activate(cmd);

               ssaoPass->SetSRV(cmd, "gDepth", *cameraContext.depth);
               ssaoPass->SetSRV(cmd, "gRandomDirs", *ssaoRandomDirs);
               ssaoPass->SetSRV(cmd, "gNormal", *cameraContext.normal);
               ssaoPass->SetUAV(cmd, "gSsao", *cameraContext.ssao);

               ssaoPass->Dispatch2D(cmd, glm::ceil(vec2{ cameraContext.colorHDR->GetDesc().size } / vec2{ 8 }));

               ResetCS_SRV_UAV();
            }

            {
               GPU_MARKER("Color");
               PROFILE_GPU("Color");

               // baseColorPass->SetUAV(cmd, "gUnderCursorBuffer", *underCursorBuffer);
               // cmd.SetRenderTargets(cameraContext.colorHDR, cameraContext.depth);

               // todo:
               uint uavInitialCount = 0;
               cmd.pContext->OMSetRenderTargetsAndUnorderedAccessViews(1, cameraContext.colorHDR->rtv.GetAddressOf(),
                  cameraContext.depth->dsv.Get(), 1, 1, underCursorBuffer->uav.GetAddressOf(), &uavInitialCount);

               cmd.SetDepthStencilState(rendres::depthStencilStateEqual);
               cmd.SetBlendState(rendres::blendStateDefaultRGB);

               baseColorPass->SetSRV(cmd, "gSsao", *cameraContext.ssao);
               baseColorPass->SetSRV(cmd, "gDecals", *decalBuffer);
               baseColorPass->SetSRV(cmd, "gShadowMap", *cameraContext.shadowMap);
               RenderSceneAllObjects(cmd, opaqueObjs, *baseColorPass, cameraContext);
            }
         } else {
            GPU_MARKER("Color (Without ZPass)");
            PROFILE_GPU("Color (Without ZPass)");

            cmd.SetRenderTargets(cameraContext.colorHDR, cameraContext.depth);
            cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);
            cmd.SetBlendState(rendres::blendStateDefaultRGB);
            baseColorPass->SetSRV(cmd, "gSsao", *cameraContext.ssao);
            baseColorPass->SetSRV(cmd, "gDecals", *decalBuffer);

            RenderSceneAllObjects(cmd, opaqueObjs, *baseColorPass, cameraContext);
         }

         if (waterDraw) {
            {
               GPU_MARKER("Water Refraction");
               PROFILE_GPU("Water Refraction");

               cmd.CopyResource(*cameraContext.waterRefraction, *cameraContext.colorHDR);
               cmd.CopyResource(*cameraContext.depthWithoutWater, *cameraContext.depth);
            }

            GPU_MARKER("Water");
            PROFILE_GPU("Water");

            if (!waterWaves || waterRecreateWaves) {
               std::vector<WaveData> waves = GenerateWaves();

               auto bufferDesc = Buffer::Desc::Structured("waver waves", (uint)waves.size(), sizeof(WaveData));
               waterWaves = Buffer::Create(bufferDesc, waves.data());
            }

            cmd.SetRenderTargets(cameraContext.colorHDR, cameraContext.depth);
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
            waterPass->DrawInstanced(cmd, 4, waterPatchCount * waterPatchCount);
         }

         if (cfg.transparency && !transparentObjs.empty()) {
            GPU_MARKER("Transparency");
            PROFILE_GPU("Transparency");

            cmd.SetRenderTargets(cameraContext.colorHDR, cameraContext.depth);
            cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadNoWrite);
            cmd.SetBlendState(rendres::blendStateTransparency);

            if (cfg.transparencySorting) {
               // todo: slow. I assumed
               std::ranges::sort(transparentObjs, [&](RenderObject& a, RenderObject& b) {
                  float az = glm::dot(camera.Forward(), a.trans.position);
                  float bz = glm::dot(camera.Forward(), b.trans.position);
                  return az > bz;
                  });
            }

            UpdateInstanceBuffer(cmd, transparentObjs);
            RenderSceneAllObjects(cmd, transparentObjs, *baseColorPass, cameraContext);
         }

         if (1) { // todo
            GPU_MARKER("Linearize Depth");
            PROFILE_GPU("Linearize Depth");

            cmd.SetRenderTargets();

            auto linearizeDepthPass = GetGpuProgram(ProgramDesc::Cs("linearizeDepth.cs", "main"));
            linearizeDepthPass->Activate(cmd);

            linearizeDepthPass->SetSRV(cmd, "gDepth", *cameraContext.depth);
            linearizeDepthPass->SetUAV(cmd, "gDepthOut", cameraContext.linearDepth->GetMipUav(0));

            linearizeDepthPass->Dispatch2D(cmd, cameraContext.depth->GetDesc().size, int2{ 8 });

            ResetCS_SRV_UAV();
         }

         if (depthDownsampleEnable) {
            GPU_MARKER("Downsample Depth");
            PROFILE_GPU("Downsample  Depth");

            cmd.SetRenderTargets();

            auto downsampleDepthPass = GetGpuProgram(ProgramDesc::Cs("linearizeDepth.cs", "downsampleDepth"));
            downsampleDepthPass->Activate(cmd);

            auto& texture = cameraContext.linearDepth;

            int nMips = texture->GetDesc().mips;
            int2 size = texture->GetDesc().size;

            for (int iMip = 0; iMip < nMips - 1; ++iMip) {
               size /= 2;

               downsampleDepthPass->SetSRV(cmd, "gDepth", texture->GetMipSrv(iMip));
               downsampleDepthPass->SetUAV(cmd, "gDepthOut", texture->GetMipUav(iMip + 1));

               downsampleDepthPass->Dispatch2D(cmd, size, int2{ 8 });

               ResetCS_SRV_UAV();
            }
         }

         if (applyFog) {
            GPU_MARKER("Fog");
            PROFILE_GPU("Fog");

            cmd.SetRenderTargets();

            auto fogPass = GetGpuProgram(ProgramDesc::Cs("fog.cs", "main"));

            fogPass->Activate(cmd);

            fogPass->SetSRV(cmd, "gLights", *lightBuffer);
            fogPass->SetSRV(cmd, "gShadowMap", *cameraContext.shadowMap);

            fogPass->SetSRV(cmd, "gDepth", *cameraContext.depth);
            fogPass->SetUAV(cmd, "gColor", *cameraContext.colorHDR);

            fogPass->Dispatch2D(cmd, cameraContext.colorHDR->GetDesc().size, int2{ 8 });

            ResetCS_SRV_UAV();
         }
      }

      // if (cfg.ssao)
      {
         GPU_MARKER("Tonemap");
         PROFILE_GPU("Tonemap");

         cmd.SetRenderTargets();

         auto tonemapPass = GetGpuProgram(ProgramDesc::Cs("tonemap.cs", "main"));

         tonemapPass->Activate(cmd);

         tonemapPass->SetSRV(cmd, "gColorHDR", *cameraContext.colorHDR);
         tonemapPass->SetUAV(cmd, "gColorLDR", *cameraContext.colorLDR);

         tonemapPass->Dispatch2D(cmd, cameraContext.colorHDR->GetDesc().size, int2{ 8 });

         ResetCS_SRV_UAV();
      }

      if (dbgRenderEnable) {
         GPU_MARKER("Dbg Rend");
         PROFILE_GPU("Dbg Rend");

         cmd.SetRenderTargets(cameraContext.colorLDR, cameraContext.depth);

         DbgRend& dbgRend = *scene.dbgRend;
         dbgRend.Clear();

         // int size = 20;
         //
         // for (int i = -size; i <= size; ++i) {
         //    dbgRend.DrawLine(vec3{ i, 0, -size }, vec3{ i, 0, size });
         // }
         //
         // for (int i = -size; i <= size; ++i) {
         //    dbgRend.DrawLine(vec3{ -size, 0, i }, vec3{ size, 0, i });
         // }

         // AABB aabb{ vec3_One, vec3_One * 3.f };
         // dbgRend.DrawAABB(aabb);

         // auto shadowInvViewProjection = glm::inverse(shadowCamera.GetViewProjection());
         // dbgRend.DrawViewProjection(shadowInvViewProjection);

         for (auto [e, trans, light] : scene.GetEntitiesWith<SceneTransformComponent, LightComponent>().each()) {
            dbgRend.DrawSphere({ trans.position, light.radius }, vec4{ light.color, 1 });
         }

         dbgRend.Render(cmd, camera);
      }
   }

   void Renderer::RenderSceneAllObjects(CommandList& cmd, const std::vector<RenderObject>& renderObjs,
      GpuProgram& program, const CameraContext& cameraContext) {
      program.Activate(cmd);
      program.SetSRV(cmd, "gInstances", *instanceBuffer);
      program.SetSRV(cmd, "gLights", *lightBuffer);

      // set mesh
      auto* context = cmd.pContext;
      context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      ID3D11InputLayout* inputLayout = rendres::GetInputLayout(baseColorPass->vs->blob.Get(), VertexPosNormal::inputElementDesc);
      context->IASetInputLayout(inputLayout);

      ID3D11Buffer* vBuffer = mesh.vertexBuffer->GetBuffer();
      uint offset = 0;
      context->IASetVertexBuffers(0, 1, &vBuffer, &mesh.geom.nVertexByteSize, &offset);
      context->IASetIndexBuffer(mesh.indexBuffer->GetBuffer(), DXGI_FORMAT_R16_UINT, 0);
      //

      int instanceID = 0;

      for (const auto& [trans, material] : renderObjs) {
         SDrawCallCB cb;
         cb.instance.transform = glm::translate(mat4(1), trans.position);
         cb.instance.transform = glm::transpose(cb.instance.transform);
         cb.instance.material.roughness = material.roughness;
         cb.instance.material.albedo = material.albedo;
         cb.instance.material.metallic = material.metallic;
         cb.instance.entityID = (uint)trans.entity.GetID();
         cb.instanceStart = instanceID++;

         auto dynCB = cmd.AllocDynConstantBuffer(cb);
         program.SetCB<SDrawCallCB>(cmd, "gDrawCallCB", *dynCB.buffer, dynCB.offset);

         if (instancedDraw) {
            if (indirectDraw) {
               // DrawIndexedInstancedArgs args{ (uint)mesh.geom.IndexCount(), (uint)renderObjs.size(), 0, 0, 0 };
               DrawIndexedInstancedArgs args{ (uint)mesh.geom.IndexCount(), 0, 0, 0, 0 };
               auto dynArgs = cmd.AllocDynDrawIndexedInstancedBuffer(&args, 1);

               if (1) {
                  auto indirectArgsTest = GetGpuProgram(ProgramDesc::Cs("cull.cs", "indirectArgsTest"));
               
                  indirectArgsTest->Activate(cmd);
               
                  indirectArgsTest->SetUAV(cmd, "gIndirectArgsInstanceCount", *dynArgs.buffer);

                  uint4 offset{ dynArgs.offset / sizeof(uint), (uint)renderObjs.size(), 0, 0};
                  auto testCB = cmd.AllocDynConstantBuffer(offset);
                  indirectArgsTest->SetCB<uint4>(cmd, "gTestCB", *testCB.buffer, testCB.offset);
               
                  indirectArgsTest->Dispatch1D(cmd, 1);
               }

               program.DrawIndexedInstancedIndirect(cmd, *dynArgs.buffer, dynArgs.offset);
            } else {
               program.DrawIndexedInstanced(cmd, mesh.geom.IndexCount(), (uint)renderObjs.size());
            }
            break;
         } else {
            // program->DrawInstanced(cmd, mesh.geom.VertexCount());
            program.DrawIndexedInstanced(cmd, mesh.geom.IndexCount());
         }
      }
   }

   uint Renderer::GetEntityIDUnderCursor(CommandList& cmd) {
      uint entityID;
      cmd.ReadbackBuffer(*underCursorBuffer, 0, sizeof(uint), &entityID);

      return entityID;
   }

}
