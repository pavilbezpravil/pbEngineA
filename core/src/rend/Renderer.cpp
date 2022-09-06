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

   CVarValue<bool> instancedDraw{ "render/instanced draw", true };
   CVarValue<bool> applyFog{ "render/apply fog", true };
   CVarValue<bool> rayTracingSceneRender{ "render/ray tracing scene render", false };
   CVarValue<bool> animationTimeUpdate{ "render/animation time update", true };

   CVarSlider<int> fogNSteps{ "render/fog/nSteps", 8, 0, 128 };

   CVarValue<bool> waterWireframe{ "render/water/wireframe", false };
   CVarSlider<float> waterTessFactorEdge{ "render/water/tess factor edge", 32.f, 0.f, 64.f };
   CVarSlider<float> waterTessFactorInside{ "render/water/tess factor inside", 32.f, 0.f, 64.f };
   CVarSlider<float> waterPatchSize{ "render/water/patch size", 4.f, 1.f, 32.f };
   CVarSlider<int> waterPatchCount{ "render/water/patch count", 32, 1, 64 };
   CVarSlider<int> waterNWaves{ "render/water/nWaves", 16, 1, 64 };
   CVarSlider<float> waterMaxWavelength{ "render/water/wavelength max", 16.f, 0.1f, 64.f };
   CVarSlider<float> waterWavelengthAmplitudeRatio{ "render/water/wavelength-amplitude ratio", 100.f, 10.0f, 300.f };
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
   }

   Renderer::~Renderer() {
      rendres::Term();
   }

   void Renderer::Init() {
      rendres::Init(); // todo:

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

      programDesc = ProgramDesc::VsHsDsPs("water.hlsl", "waterVS", "waterHS", "waterDS", "waterPS");
      waterPass = GpuProgram::Create(programDesc);

      programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "ps_main");
      programDesc.ps.defines.AddDefine("DECAL");
      baseDecal = GpuProgram::Create(programDesc);

      programDesc = ProgramDesc::Cs("ssao.cs", "main");
      ssaoPass = GpuProgram::Create(programDesc);

      programDesc = ProgramDesc::Cs("fog.cs", "main");
      fogPass = GpuProgram::Create(programDesc);

      programDesc = ProgramDesc::Cs("tonemap.cs", "main");
      tonemapPass = GpuProgram::Create(programDesc);

      mesh = Mesh::Create(MeshGeomCube());
   }

   void Renderer::UpdateInstanceBuffer(CommandList& cmd, const std::vector<RenderObject>& renderObjs) {
      if (!instanceBuffer || instanceBuffer->ElementsCount() < renderObjs.size()) {
         auto bufferDesc = Buffer::Desc::Structured("instance buffer", (uint)renderObjs.size(), sizeof(Instance));
         instanceBuffer = Buffer::Create(bufferDesc);
      }

      std::vector<Instance> instances;
      instances.reserve(renderObjs.size());
      for (auto& [trans, material] : renderObjs) {
         mat4 transform = glm::transpose(trans.GetMatrix());

         Material m;
         m.albedo = material.albedo;
         m.roughness = material.roughness;
         m.metallic = material.metallic;

         instances.emplace_back(transform, m);
      }

      cmd.UpdateSubresource(*instanceBuffer, instances.data(), 0, instances.size() * sizeof(Instance));
   }

   void Renderer::RenderDataPrepare(CommandList& cmd, Scene& scene) {
      opaqueObjs.clear();
      transparentObjs.clear();

      for (auto [e, sceneTrans, material] : scene.GetEntitiesWith<SceneTransformComponent, SimpleMaterialComponent>().each()) {
         if (material.opaque) {
            opaqueObjs.emplace_back(sceneTrans, material);
         } else {
            transparentObjs.emplace_back(sceneTrans, material);
         }
      }

      if (!instanceBuffer || instanceBuffer->ElementsCount() < scene.EntitiesCount()) {
         auto bufferDesc = Buffer::Desc::Structured("instance buffer", scene.EntitiesCount(), sizeof(Instance));
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

      sceneCB.nWaves = waterNWaves;
      sceneCB.fogNSteps = fogNSteps;

      sceneCB.nLights = (int)scene.GetEntitiesWith<LightComponent>().size();
      sceneCB.nDecals = (int)nDecals;

      sceneCB.tessFactorEdge = waterTessFactorEdge;
      sceneCB.tessFactorInside = waterTessFactorInside;
      sceneCB.waterPatchSize = waterPatchSize;
      sceneCB.waterPatchCount = waterPatchCount;

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

      SCameraCB cameraCB;
      camera.FillSCameraCB(cameraCB);
      cameraCB.rtSize = cameraContext.colorHDR->GetDesc().size;

      cmd.AllocAndSetCommonCB(CB_SLOT_CAMERA, cameraCB);

      // todo:
      auto ResetCS_SRV_UAV = [&] {
         ID3D11ShaderResourceView* views[] = { nullptr };
         cmd.pContext->CSSetShaderResources(0, 1, views);

         ID3D11UnorderedAccessView* viewsUAV[] = { nullptr };
         cmd.pContext->CSSetUnorderedAccessViews(0, 1, viewsUAV, nullptr);
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

               ssaoPass->Activate(cmd);

               ssaoPass->SetSRV(cmd, "gDepth", *cameraContext.depth);
               ssaoPass->SetSRV(cmd, "gRandomDirs", *ssaoRandomDirs);
               ssaoPass->SetSRV(cmd, "gNormal", *cameraContext.normal);
               ssaoPass->SetUAV(cmd, "gSsao", *cameraContext.ssao);

               ssaoPass->Dispatch(cmd, glm::ceil(vec2{ cameraContext.colorHDR->GetDesc().size } / vec2{ 8 }));

               ResetCS_SRV_UAV();
            }

            {
               GPU_MARKER("Color");
               PROFILE_GPU("Color");

               cmd.SetRenderTargets(cameraContext.colorHDR, cameraContext.depth);
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

         {
            GPU_MARKER("Water");
            PROFILE_GPU("Water");

            if (!waterWaves || waterWaves->ElementsCount() != waterNWaves || waterRecreateWaves) {
               std::vector<WaveData> waves;
               waves.reserve(waterNWaves);

               for (int i = 0; i < waterNWaves; ++i) {
                  WaveData wave;

                  // shallow water v = sqrt(g * h)
                  // deep water v = sqrt((g * wavelength) / PI2)
                  float wavelength = Random::Uniform(0.f, waterMaxWavelength);
                  float g = 9.8f;
                  float speed = sqrt((g * wavelength) / PI2);

                  wave.direction = glm::normalize(Random::UniformInCircle());
                  wave.amplitude = wavelength / waterWavelengthAmplitudeRatio * Random::Uniform(0.5f, 1.5f);
                  wave.length = wavelength; // todo:

                  wave.magnitude = PI2 / wavelength;
                  // wave.frequency = speed / wavelength;
                  wave.frequency = sqrt((g * PI2) / wavelength);
                  wave.phase = Random::Uniform(0.f, PI2);
                  wave.steepness = 1;
                  waves.emplace_back(wave);
               }

               auto bufferDesc = Buffer::Desc::Structured("waver waves", waterNWaves, sizeof(WaveData));
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

            waterPass->Activate(cmd);

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

         if (applyFog) {
            GPU_MARKER("Fog");
            PROFILE_GPU("Fog");

            cmd.SetRenderTargets();

            fogPass->Activate(cmd);

            fogPass->SetSRV(cmd, "gLights", *lightBuffer);
            fogPass->SetSRV(cmd, "gShadowMap", *cameraContext.shadowMap);

            fogPass->SetSRV(cmd, "gDepth", *cameraContext.depth);
            fogPass->SetUAV(cmd, "gColor", *cameraContext.colorHDR);

            fogPass->Dispatch(cmd, cameraContext.colorHDR->GetDesc().size, int2{ 8 });

            ResetCS_SRV_UAV();
         }
      }

      // if (cfg.ssao)
      {
         GPU_MARKER("Tonemap");
         PROFILE_GPU("Tonemap");

         cmd.SetRenderTargets();

         tonemapPass->Activate(cmd);

         tonemapPass->SetSRV(cmd, "gColorHDR", *cameraContext.colorHDR);
         tonemapPass->SetUAV(cmd, "gColorLDR", *cameraContext.colorLDR);

         tonemapPass->Dispatch(cmd, cameraContext.colorHDR->GetDesc().size, int2{ 8 });

         ResetCS_SRV_UAV();
      }

      if (1) {
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
         cb.transform = glm::translate(mat4(1), trans.position);
         cb.transform = glm::transpose(cb.transform);
         cb.material.roughness = material.roughness;
         cb.material.albedo = material.albedo;
         cb.material.metallic = material.metallic;
         cb.instanceStart = instanceID++;

         auto dynCB = cmd.AllocDynConstantBuffer(cb);
         program.SetCB<SDrawCallCB>(cmd, "gDrawCallCB", *dynCB.buffer, dynCB.offset);

         // if (cfg.useInstancedDraw) {
         if (instancedDraw) {
            program.DrawIndexedInstanced(cmd, mesh.geom.IndexCount(), (int)renderObjs.size());
            break;
         } else {
            // program->DrawInstanced(cmd, mesh.geom.VertexCount());
            program.DrawIndexedInstanced(cmd, mesh.geom.IndexCount());
         }
      }
   }

}
