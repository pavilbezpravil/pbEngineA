#include "pch.h"
#include "Renderer.h"

#include <NvBlastTkActor.h>

#include "DbgRend.h"
#include "RendRes.h"
#include "RTRenderer.h"
#include "core/CVar.h"
#include "core/Profiler.h"
#include "math/Random.h"
#include "math/Shape.h"
#include "physics/PhysComponents.h"

#include "shared/hlslCppShared.hlsli"
#include "system/Water.h"


namespace pbe {

   CVarValue<bool> cFreezeCullCamera{ "render/freeze cull camera", false };
   CVarValue<bool> cUseFrustumCulling{ "render/use frustum culling", false };

   CVarValue<bool> cvRenderDecals{ "render/decals", true };
   CVarValue<bool> cvRenderOpaqueSort{ "render/opaque sort", true };
   CVarValue<bool> cvRenderShadowMap{ "render/shadow map", true };
   CVarValue<bool> cvRenderZPass{ "render/z pass", true };
   CVarValue<bool> cvRenderSsao{ "render/ssao", false };
   CVarValue<bool> cvRenderTransparency{ "render/transparency", true };
   CVarValue<bool> cvRenderTransparencySort{ "render/transparency sort", true };

   CVarValue<bool> dbgRenderEnable{ "render/debug render", true };
   CVarValue<bool> instancedDraw{ "render/instanced draw", true };
   CVarValue<bool> cvOutlineEnable{ "render/outline", true };
   CVarValue<bool> indirectDraw{ "render/indirect draw", false };
   CVarValue<bool> depthDownsampleEnable{ "render/depth downsample enable", true };
   CVarValue<bool> rayTracingSceneRender{ "render/ray tracing scene render", true };
   CVarValue<bool> animationTimeUpdate{ "render/animation time update", true };

   CVarValue<bool> applyFog{ "render/fog/enable", true };
   CVarSlider<int> fogNSteps{ "render/fog/nSteps", 0, 0, 16 };

   CVarSlider<float> tonemapExposition{ "render/tonemap/exposion", 1.f, 0.f, 3.f };

   static mat4 NDCToTexSpaceMat4() {
      mat4 scale = glm::scale(mat4{1.f}, {0.5f, -0.5f, 1.f});
      mat4 translate = glm::translate(mat4{1.f}, {0.5f, 0.5f, 0.f});

      return translate * scale;
   }

   vec2 UVToNDC(const vec2& uv) {
      vec2 ndc = uv * 2.f - 1.f;
      ndc.y = -ndc.y;
      return ndc;
   }

   vec2 NDCToUV(const vec2& ndc) {
      vec2 uv = ndc;
      uv.y = -uv.y;
      uv = (uv + 1.f) * 0.5f;
      return uv;
   }

   vec3 GetWorldPosFromUV(const vec2& uv, const mat4& invViewProj) {
      vec2 ndc = UVToNDC(uv);

      vec4 worldPos4 = invViewProj * vec4(ndc, 0, 1);
      vec3 worldPos = worldPos4 / worldPos4.w;

      return worldPos;
   }

   void RenderCamera::NextFrame() {
      prevView = view;
      prevProjection = projection;
   }

   void RenderCamera::FillSCameraCB(SCameraCB& cameraCB) const {
      cameraCB.view = view;
      cameraCB.invView = glm::inverse(cameraCB.view);

      cameraCB.projection = projection;

      cameraCB.viewProjection = GetViewProjection();
      cameraCB.invViewProjection = cameraCB.viewProjection;

      cameraCB.prevViewProjection = GetPrevViewProjection();
      cameraCB.prevInvViewProjection = cameraCB.prevViewProjection;

      auto frustumCornerLeftUp = glm::inverse(projection) * vec4{ 1, 1, 0, 1 };
      frustumCornerLeftUp /= frustumCornerLeftUp.w;
      frustumCornerLeftUp /= frustumCornerLeftUp.z; // to 1 z plane

      // todo: remove
      // cameraCB.frustumCornerLeftUp = vec2{ frustumCornerLeftUp.x, frustumCornerLeftUp.y };
      cameraCB.frustumSize = vec2{ frustumCornerLeftUp.x, frustumCornerLeftUp.y };

      // todo:
      cameraCB.position = position;
      cameraCB.forward = Forward();

      cameraCB.zNear = zNear;
      cameraCB.zFar = zFar;

      Frustum frustum{GetViewProjection() };
      memcpy(cameraCB.frustumPlanes, frustum.planes, sizeof(frustum.planes));
   }

   RenderContext CreateRenderContext(int2 size) {
      RenderContext context;

      Texture2D::Desc texDesc;
      texDesc.size = size;

      texDesc.name = "scene colorHDR";
      texDesc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      // texDesc.format = DXGI_FORMAT_R11G11B10_FLOAT; // my laptop doesnot support this format as UAV
      texDesc.bindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
      texDesc.bindFlags |= D3D11_BIND_UNORDERED_ACCESS; // todo:
      context.colorHDR = Texture2D::Create(texDesc);

      texDesc.name = "scene colorLDR";
      // texDesc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      // texDesc.format = DXGI_FORMAT_R8G8B8A8_UNORM; // todo: test srgb
      context.colorLDR = Texture2D::Create(texDesc);

      texDesc.name = "water refraction";
      texDesc.bindFlags = D3D11_BIND_SHADER_RESOURCE;
      context.waterRefraction = Texture2D::Create(texDesc);

      texDesc.name = "scene depth";
      // texDesc.format = DXGI_FORMAT_D24_UNORM_S8_UINT;
      texDesc.format = DXGI_FORMAT_R24G8_TYPELESS; // todo: 32 bit
      texDesc.bindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
      context.depth = Texture2D::Create(texDesc);

      texDesc.name = "scene depth without water";
      texDesc.bindFlags = D3D11_BIND_SHADER_RESOURCE;
      context.depthWithoutWater = Texture2D::Create(texDesc);

      texDesc.name = "scene linear depth";
      texDesc.format = DXGI_FORMAT_R16_FLOAT;
      texDesc.mips = 0;
      texDesc.bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
      context.linearDepth = Texture2D::Create(texDesc);

      texDesc.mips = 1;

      texDesc.name = "scene ssao";
      texDesc.format = DXGI_FORMAT_R16_UNORM;
      texDesc.bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
      context.ssao = Texture2D::Create(texDesc);

      if (!context.shadowMap) {
         Texture2D::Desc texDesc;
         texDesc.name = "shadow map";
         // texDesc.format = DXGI_FORMAT_D16_UNORM;
         texDesc.format = DXGI_FORMAT_R16_TYPELESS;
         texDesc.size = { 1024, 1024 };
         texDesc.bindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
         context.shadowMap = Texture2D::Create(texDesc);
      }

      // rt
      {
         auto& outTexture = *context.colorHDR;

         Texture2D::Desc texDesc {
            .size = size,
            // .format = DXGI_FORMAT_R8G8B8A8_UNORM, // todo:
            // .format = DXGI_FORMAT_R11G11B10_FLOAT, // todo: mb place metallic to diff rt?
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT, // todo: too match
            .bindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            .name = "scene base color",
         };
         context.baseColorTex = Texture2D::Create(texDesc);

         texDesc = {
            .size = size,
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .bindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            .name = "scene motion",
         };
         context.motionTex = Texture2D::Create(texDesc);

         texDesc = {
            .size = size,
            // .format = DXGI_FORMAT_R10G10B10A2_UNORM,
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT, // cant read from UNORM as UAV
            .bindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            .name = "scene normal",
         };
         context.normalTex = Texture2D::Create(texDesc);

         texDesc = {
            .size = size,
            .format = DXGI_FORMAT_R32_FLOAT,
            .bindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            .name = "scene view z",
         };
         context.viewz = Texture2D::Create(texDesc);

         texDesc = {
            .size = size,
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
            .name = "rt diffuse",
         };
         context.diffuseTex = Texture2D::Create(texDesc);
         context.diffuseHistoryTex = Texture2D::Create(texDesc);

         texDesc = {
            .size = size,
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
            .name = "rt specular",
         };
         context.specularTex = Texture2D::Create(texDesc);
         context.specularHistoryTex = Texture2D::Create(texDesc);
      }

      texDesc = {
            .size = size,
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT, // todo: rgba8
            .bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
            .name = "outline",
      };
      context.outlineTex = Texture2D::Create(texDesc);
      texDesc.name = "outlines blurred";
      texDesc.bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
      context.outlineBlurredTex = Texture2D::Create(texDesc);

      return context;
   }

   void Renderer::Init() {
      rtRenderer.reset(new RTRenderer());

      auto programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "ps_main");
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
         SMaterial m;
         m.baseColor = material.baseColor;
         m.roughness = material.roughness;
         m.metallic = material.metallic;
         m.emissivePower = material.emissivePower;

         SInstance instance;
         instance.transform = trans.GetWorldMatrix();
         instance.prevTransform = trans.GetPrevMatrix();
         instance.material = m;
         instance.entityID = (uint)trans.entity.GetID();

         instances.emplace_back(instance);
      }

      cmd.UpdateSubresource(*instanceBuffer, instances.data(), 0, instances.size() * sizeof(SInstance));
   }

   void Renderer::RenderDataPrepare(CommandList& cmd, const Scene& scene, const RenderCamera& cullCamera) {
      opaqueObjs.clear();
      transparentObjs.clear();

      // todo:
      Frustum frustum{ cullCamera.GetViewProjection() };

      for (auto [e, sceneTrans, material] :
         scene.View<SceneTransformComponent, MaterialComponent>().each()) {

         // DrawDesc desc;
         // desc.entityID = (uint)e;
         // desc.transform = sceneTrans.GetMatrix();
         // desc.material = { material, true };
         //
         // drawDescs.push_back(desc);

         if (cUseFrustumCulling) {
            vec3 scale = sceneTrans.Scale();
            // todo:
            float sphereRadius = glm::max(scale.x, scale.y);
            sphereRadius = glm::max(sphereRadius, scale.z) * 2;
            if (!frustum.SphereTest({ sceneTrans.Position(), })) {
               continue;
            }
         }

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
         auto nLights = (uint)scene.CountEntitiesWithComponents<LightComponent>();
         if (!lightBuffer || lightBuffer->ElementsCount() < nLights) {
            auto bufferDesc = Buffer::Desc::Structured("light buffer", nLights, sizeof(SLight));
            lightBuffer = Buffer::Create(bufferDesc);
         }

         std::vector<SLight> lights;
         lights.reserve(nLights);

         for (auto [e, trans, light] : scene.View<SceneTransformComponent, LightComponent>().each()) {
            SLight l;
            l.position = trans.Position();
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
               test = Random::Float3(vec3{ -1 }, vec3{ 1 });
            } while (glm::length(test) > 1);

            dirs.emplace_back(test);
         }

         cmd.UpdateSubresource(*ssaoRandomDirs, dirs.data(), 0, dirs.size() * sizeof(vec3));
      }
   }

   void Renderer::RenderScene(CommandList& cmd, const Scene& scene, const RenderCamera& camera, RenderContext& context) {
      if (!baseColorPass->Valid()) {
         return;
      }

      GPU_MARKER("Render Scene");
      PROFILE_GPU("Render Scene");
      PIX_EVENT_SYSTEM(Render, "Render Scene");

      static RenderCamera cullCamera = camera;
      if (!cFreezeCullCamera) {
         cullCamera = camera;
      }

      RenderDataPrepare(cmd, scene, cullCamera);

      // todo: may be skipped
      cmd.ClearRenderTarget(*context.colorLDR, vec4{0, 0, 0, 1});
      cmd.ClearRenderTarget(*context.colorHDR, vec4{0, 0, 0, 1});
      cmd.ClearRenderTarget(*context.normalTex, vec4{0, 0, 0, 0});
      cmd.ClearRenderTarget(*context.baseColorTex, vec4{ 0, 0, 0, 1 });
      cmd.ClearRenderTarget(*context.motionTex, vec4{ 0, 0, 0, 0 });
      cmd.ClearRenderTarget(*context.outlineTex, vec4{ 0, 0, 0, 0 });

      // todo: NRD dont work with black textures
      // todo: only in editor
      // cmd.ClearUAVFloat(*context.diffuseHistoryTex, vec4{ 0, 0, 0, 0 });
      // cmd.ClearUAVFloat(*context.specularHistoryTex, vec4{ 0, 0, 0, 0 });

      cmd.ClearRenderTarget(*context.viewz, vec4{ 1000000.0f, 0, 0, 0 }); // todo: why it defaults for NRD?
      cmd.ClearDepthTarget(*context.depth, 1);

      cmd.SetRasterizerState(rendres::rasterizerState);

      uint nDecals = 0;
      if (cvRenderDecals) {
         std::vector<SDecal> decals;

         MaterialComponent decalDefault{}; // todo:
         for (auto [e, trans, decal] : scene.View<SceneTransformComponent, DecalComponent>().each()) {
            decalObjs.emplace_back(trans, decalDefault);

            vec3 size = trans.Scale() * 0.5f;

            mat4 view = glm::lookAt(trans.Position(), trans.Position() + trans.Forward(), trans.Up());
            mat4 projection = glm::ortho(-size.x, size.x, -size.y, size.y, -size.z, size.z);
            mat4 viewProjection = projection * view;
            decals.emplace_back(viewProjection, decal.baseColor, decal.metallic, decal.roughness);
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

      sceneCB.fogNSteps = fogNSteps;

      sceneCB.nLights = scene.CountEntitiesWithComponents<LightComponent>();
      sceneCB.nDecals = (int)nDecals;

      sceneCB.exposition = tonemapExposition;

      sceneCB.directLight.color = {};
      sceneCB.directLight.direction = vec3{1, 0, 0};
      sceneCB.directLight.type = SLIGHT_TYPE_DIRECT;

      bool hasDirectLight = false;
      if (Entity directEntity = scene.GetAnyWithComponent<DirectLightComponent>()) {
         hasDirectLight = true;

         auto& trans = directEntity.GetTransform();
         auto& directLight = directEntity.Get<DirectLightComponent>();

         sceneCB.directLight.color = directLight.color * directLight.intensity;
         sceneCB.directLight.direction = trans.Forward();

         const float halfSize = 25;
         const float halfDepth = 50;

         auto shadowSpace = glm::lookAt({}, sceneCB.directLight.direction, trans.Up());
         vec3 posShadowSpace = shadowSpace * vec4(camera.position, 1);

         vec2 shadowMapTexels = context.shadowMap->GetDesc().size;
         vec3 shadowTexelSize = vec3{ 2.f * halfSize / shadowMapTexels, 2.f * halfDepth / (1 << 16) };
         vec3 snappedPosShadowSpace = glm::ceil(posShadowSpace / shadowTexelSize) * shadowTexelSize;

         vec3 snappedPosW = glm::inverse(shadowSpace) * vec4(snappedPosShadowSpace, 1);

         shadowCamera.position = snappedPosW;

         shadowCamera.projection = glm::ortho<float>(-halfSize, halfSize, -halfSize, halfSize, -halfDepth, halfDepth);
         shadowCamera.view = glm::lookAt(shadowCamera.position, shadowCamera.position + sceneCB.directLight.direction, trans.Up());
         sceneCB.toShadowSpace = NDCToTexSpaceMat4() * shadowCamera.GetViewProjection();
      }

      if (Entity skyEntity = scene.GetAnyWithComponent<SkyComponent>()) {
         const auto& sky = skyEntity.Get<SkyComponent>();

         sceneCB.skyIntensity = sky.intensity;
      } else {
         sceneCB.skyIntensity = 0;
      }

      cmd.AllocAndSetCB({ CB_SLOT_SCENE }, sceneCB);

      if (cvRenderOpaqueSort) {
         // todo: slow. I assumed
         std::ranges::sort(opaqueObjs, [&](const RenderObject& a, const RenderObject& b) {
            float az = glm::dot(camera.Forward(), a.trans.Position());
            float bz = glm::dot(camera.Forward(), b.trans.Position());
            return az < bz;
         });
      }
      UpdateInstanceBuffer(cmd, opaqueObjs);

      cmd.pContext->ClearUnorderedAccessViewFloat(context.ssao->uav.Get(), &vec4_One.x);

      uint4 clearValue = uint4{ (uint)-1 };
      cmd.pContext->ClearUnorderedAccessViewUint(underCursorBuffer->uav.Get(), &clearValue.x);

      context.underCursorBuffer = underCursorBuffer;

      SCameraCB cameraCB;
      camera.FillSCameraCB(cameraCB);
      cameraCB.rtSize = context.colorHDR->GetDesc().size;
      cmd.AllocAndSetCB({ CB_SLOT_CAMERA }, cameraCB);

      {
         SCameraCB cullCameraCB;
         cullCamera.FillSCameraCB(cullCameraCB);
         cullCameraCB.rtSize = context.colorHDR->GetDesc().size;
         cmd.AllocAndSetCB({ CB_SLOT_CULL_CAMERA }, cullCameraCB);
      }

      SEditorCB editorCB;
      editorCB.cursorPixelIdx = context.cursorPixelIdx;
      cmd.AllocAndSetCB({ CB_SLOT_EDITOR }, editorCB);

      // todo:
      auto ResetCS_SRV_UAV = [&] {
         ID3D11ShaderResourceView* viewsSRV[] = { nullptr, nullptr };
         cmd.pContext->CSSetShaderResources(0, _countof(viewsSRV), viewsSRV);

         ID3D11UnorderedAccessView* viewsUAV[] = { nullptr, nullptr };
         cmd.pContext->CSSetUnorderedAccessViews(0, _countof(viewsUAV), viewsUAV, nullptr);
      };

      cmd.SetSRV({ SRV_SLOT_LIGHTS }, lightBuffer);

      if (rayTracingSceneRender) {
         cmd.SetViewport({}, context.colorHDR->GetDesc().size);
         cmd.SetBlendState(rendres::blendStateDefaultRGBA);

         {
            GPU_MARKER("ZPass");
            PROFILE_GPU("ZPass");
         
            // todo: without normal?
            cmd.SetRenderTargets(context.normalTex, context.depth);
            cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);
         
            auto programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "ps_main");
            programDesc.vs.defines.AddDefine("ZPASS");
            programDesc.ps.defines.AddDefine("ZPASS");
            auto baseZPass = GetGpuProgram(programDesc);
         
            RenderSceneAllObjects(cmd, opaqueObjs, *baseZPass);
         }

         {
            GPU_MARKER("GBuffer");
            PROFILE_GPU("GBuffer");

            Texture2D* rts[] = { context.colorHDR, context.baseColorTex,
               context.normalTex, context.motionTex, context.viewz };
            uint nRts = _countof(rts);
            cmd.SetRenderTargets(nRts, rts, context.depth);
            cmd.SetPsUAV(nRts, context.underCursorBuffer);

            cmd.SetViewport({}, context.depth->GetDesc().size);

            cmd.SetDepthStencilState(rendres::depthStencilStateEqual);

            auto programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "ps_main");
            programDesc.vs.defines.AddDefine("GBUFFER");
            programDesc.ps.defines.AddDefine("GBUFFER");

            auto baseZPass = GetGpuProgram(programDesc);
            RenderSceneAllObjects(cmd, opaqueObjs, *baseZPass);

            cmd.SetRenderTargets();
         }

         rtRenderer->RenderScene(cmd, scene, camera, context);
         ResetCS_SRV_UAV();
      } else {
         if (cvRenderShadowMap && hasDirectLight) {
            GPU_MARKER("Shadow Map");
            PROFILE_GPU("Shadow Map");

            cmd.ClearDepthTarget(*context.shadowMap, 1);

            cmd.SetRenderTargets(nullptr, context.shadowMap);
            cmd.SetViewport({}, context.shadowMap->GetDesc().size);
            cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);
            cmd.SetBlendState(rendres::blendStateDefaultRGBA);

            cmd.pContext->PSSetSamplers(0, 1, &rendres::samplerStateWrapPoint); // todo:

            SCameraCB shadowCameraCB;
            shadowCamera.FillSCameraCB(shadowCameraCB);
            // shadowCameraCB.rtSize = context.colorHDR->GetDesc().size;

            cmd.AllocAndSetCB({ CB_SLOT_CAMERA }, shadowCameraCB);

            auto programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main");
            programDesc.vs.defines.AddDefine("ZPASS");
            auto shadowMapPass = GetGpuProgram(programDesc);
            RenderSceneAllObjects(cmd, opaqueObjs, *shadowMapPass);

            cmd.SetRenderTargets();
            cmd.SetSRV({ SRV_SLOT_SHADOWMAP }, context.shadowMap);
         }

         cmd.AllocAndSetCB({ CB_SLOT_CAMERA }, cameraCB); // todo: set twice

         cmd.SetViewport({}, context.colorHDR->GetDesc().size); /// todo:

         if (cvRenderZPass) {
            {
               GPU_MARKER("ZPass");
               PROFILE_GPU("ZPass");

               cmd.SetRenderTargets(context.normalTex, context.depth);
               cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);
               cmd.SetBlendState(rendres::blendStateDefaultRGBA);

               auto programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "ps_main");
               programDesc.vs.defines.AddDefine("ZPASS");
               programDesc.ps.defines.AddDefine("ZPASS");
               auto baseZPass = GetGpuProgram(programDesc);

               RenderSceneAllObjects(cmd, opaqueObjs, *baseZPass);
            }

            if (cvRenderSsao) {
               GPU_MARKER("SSAO");
               PROFILE_GPU("SSAO");

               cmd.SetRenderTargets();

               auto ssaoPass = GetGpuProgram(ProgramDesc::Cs("ssao.hlsl", "main"));

               ssaoPass->Activate(cmd);

               ssaoPass->SetSRV(cmd, "gDepth", *context.depth);
               ssaoPass->SetSRV(cmd, "gRandomDirs", *ssaoRandomDirs);
               ssaoPass->SetSRV(cmd, "gNormal", *context.normalTex);
               ssaoPass->SetUAV(cmd, "gSsao", *context.ssao);

               cmd.Dispatch2D(glm::ceil(vec2{ context.colorHDR->GetDesc().size } / vec2{ 8 }));

               ResetCS_SRV_UAV();
            }

            {
               GPU_MARKER("Color");
               PROFILE_GPU("Color");

               // baseColorPass->SetUAV(cmd, "gUnderCursorBuffer", *underCursorBuffer);
               // cmd.SetRenderTargets(context.colorHDR, context.depth);

               cmd.SetRenderTargetsUAV(context.colorHDR, context.depth, underCursorBuffer);

               cmd.SetDepthStencilState(rendres::depthStencilStateEqual);
               cmd.SetBlendState(rendres::blendStateDefaultRGB);

               baseColorPass->SetSRV(cmd, "gSsao", *context.ssao);
               baseColorPass->SetSRV(cmd, "gDecals", *decalBuffer);
               RenderSceneAllObjects(cmd, opaqueObjs, *baseColorPass);
            }
         } else {
            GPU_MARKER("Color (Without ZPass)");
            PROFILE_GPU("Color (Without ZPass)");

            cmd.SetRenderTargets(context.colorHDR, context.depth);
            cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);
            cmd.SetBlendState(rendres::blendStateDefaultRGB);
            baseColorPass->SetSRV(cmd, "gSsao", *context.ssao);
            baseColorPass->SetSRV(cmd, "gDecals", *decalBuffer);

            RenderSceneAllObjects(cmd, opaqueObjs, *baseColorPass);
         }

         terrainSystem.Render(cmd, scene, context);

         waterSystem.Render(cmd, scene, context);

         if (cvRenderTransparency && !transparentObjs.empty()) {
            GPU_MARKER("Transparency");
            PROFILE_GPU("Transparency");

            cmd.SetRenderTargets(context.colorHDR, context.depth);
            cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadNoWrite);
            cmd.SetBlendState(rendres::blendStateTransparency);

            if (cvRenderTransparencySort) {
               // todo: slow. I assumed
               std::ranges::sort(transparentObjs, [&](RenderObject& a, RenderObject& b) {
                  float az = glm::dot(camera.Forward(), a.trans.Position());
                  float bz = glm::dot(camera.Forward(), b.trans.Position());
                  return az > bz;
                  });
            }

            UpdateInstanceBuffer(cmd, transparentObjs);
            RenderSceneAllObjects(cmd, transparentObjs, *baseColorPass);
         }

         if (1) { // todo
            GPU_MARKER("Linearize Depth");
            PROFILE_GPU("Linearize Depth");

            cmd.SetRenderTargets();

            auto linearizeDepthPass = GetGpuProgram(ProgramDesc::Cs("linearizeDepth.hlsl", "main"));
            linearizeDepthPass->Activate(cmd);

            linearizeDepthPass->SetSRV(cmd, "gDepth", *context.depth);
            linearizeDepthPass->SetUAV_Dx11(cmd, "gDepthOut", context.linearDepth->GetMipUav(0));

            cmd.Dispatch2D(context.depth->GetDesc().size, int2{ 8 });

            ResetCS_SRV_UAV();
         }

         if (depthDownsampleEnable) {
            GPU_MARKER("Downsample Depth");
            PROFILE_GPU("Downsample  Depth");

            cmd.SetRenderTargets();

            auto downsampleDepthPass = GetGpuProgram(ProgramDesc::Cs("linearizeDepth.hlsl", "downsampleDepth"));
            downsampleDepthPass->Activate(cmd);

            auto& texture = context.linearDepth;

            int nMips = texture->GetDesc().mips;
            int2 size = texture->GetDesc().size;

            for (int iMip = 0; iMip < nMips - 1; ++iMip) {
               size /= 2;

               downsampleDepthPass->SetSRV_Dx11(cmd, "gDepth", texture->GetMipSrv(iMip));
               downsampleDepthPass->SetUAV_Dx11(cmd, "gDepthOut", texture->GetMipUav(iMip + 1));

               cmd.Dispatch2D(size, int2{ 8 });

               ResetCS_SRV_UAV();
            }
         }

         if (applyFog) {
            GPU_MARKER("Fog");
            PROFILE_GPU("Fog");

            cmd.SetRenderTargets();

            auto fogPass = GetGpuProgram(ProgramDesc::Cs("fog.hlsl", "main"));

            fogPass->Activate(cmd);

            fogPass->SetSRV(cmd, "gDepth", *context.depth);
            fogPass->SetUAV(cmd, "gColor", *context.colorHDR);

            cmd.Dispatch2D(context.colorHDR->GetDesc().size, int2{ 8 });

            ResetCS_SRV_UAV();
         }
      }

      // if (cfg.ssao)
      {
         GPU_MARKER("Tonemap");
         PROFILE_GPU("Tonemap");

         cmd.SetRenderTargets();

         auto tonemapPass = GetGpuProgram(ProgramDesc::Cs("tonemap.hlsl", "main"));

         tonemapPass->Activate(cmd);

         tonemapPass->SetSRV(cmd, "gColorHDR", *context.colorHDR);
         tonemapPass->SetUAV(cmd, "gColorLDR", *context.colorLDR);

         cmd.Dispatch2D(context.colorHDR->GetDesc().size, int2{ 8 });

         ResetCS_SRV_UAV();
      }

      if (cvOutlineEnable) {
         {
            GPU_MARKER("Outline");
            PROFILE_GPU("Outline");

            cmd.SetRenderTargets(&*context.outlineTex);
            cmd.SetViewport({}, context.depth->GetDesc().size);

            RenderOutlines(cmd, scene);
         }

         {
            GPU_MARKER("Outline Blur");
            PROFILE_GPU("Outline Blur");

            cmd.SetRenderTargets();

            auto pass = GetGpuProgram(ProgramDesc::Cs("outline.hlsl", "OutlineBlurCS"));

            pass->Activate(cmd);

            pass->SetSRV(cmd, "gOutline", *context.outlineTex);
            pass->SetUAV(cmd, "gOutlineBlurOut", *context.outlineBlurredTex);

            cmd.Dispatch2D(context.outlineTex->GetDesc().size, int2{ 8 });

            ResetCS_SRV_UAV();
         }

         {
            GPU_MARKER("Outline Apply");
            PROFILE_GPU("Outline Apply");

            cmd.SetRenderTargets();

            auto pass = GetGpuProgram(ProgramDesc::Cs("outline.hlsl", "OutlineApplyCS"));

            pass->Activate(cmd);

            pass->SetSRV(cmd, "gOutline", *context.outlineTex);
            pass->SetSRV(cmd, "gOutlineBlur", *context.outlineBlurredTex);
            pass->SetUAV(cmd, "gSceneOut", *context.colorLDR);

            cmd.Dispatch2D(context.outlineTex->GetDesc().size, int2{ 8 });

            ResetCS_SRV_UAV();
         }
      }

      if (dbgRenderEnable) {
         GPU_MARKER("Dbg Rend");
         PROFILE_GPU("Dbg Rend");

         cmd.SetRenderTargets(context.colorLDR, context.depth);
         cmd.SetViewport({}, context.colorHDR->GetDesc().size); /// todo:

         DbgRend& dbgRend = *scene.dbgRend;

         // int size = 50;
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

         if (cFreezeCullCamera) {
            auto cullCameraInvViewProjection = glm::inverse(cullCamera.GetViewProjection());
            dbgRend.DrawViewProjection(cullCameraInvViewProjection);

            // dbgRend.DrawFrustum(Frustum{ cullCamera.GetViewProjection() },cullCamera.position, cullCamera.Forward());  
         }

         for (auto [e, trans, light] : scene.View<SceneTransformComponent, LightComponent>().each()) {
            dbgRend.DrawSphere({ trans.Position(), light.radius }, light.color);
         }

         for (auto [e, trans, light] : scene.View<SceneTransformComponent, TriggerComponent>().each()) {
            // todo: OBB
            // todo: box, sphere, capsule
            dbgRend.DrawAABB({ trans.Position() - trans.Scale() * 0.5f, trans.Position() + trans.Scale() * 0.5f });
         }

         for (auto [_, joint] : scene.View<JointComponent>().each()) {
            if (!joint.IsValid()) {
               continue;
            }

            dbgRend.DrawLine(joint.entity0, joint.entity1, Color_White);

            auto trans0 = joint.GetAnchorTransform(JointComponent::Anchor::Anchor0);
            auto trans1 = joint.GetAnchorTransform(JointComponent::Anchor::Anchor1);

            auto pos0 = trans0->position;
            auto pos1 = trans1->position;
            auto dir = glm::normalize(pos1 - pos0);

            auto sphere = Sphere{ pos0, 0.2f };

            Color defColor = Color_Blue;
            if (joint.type == JointType::Fixed) {
               dbgRend.DrawSphere(sphere, defColor);
            } else if (joint.type == JointType::Distance) {
               auto posMin = pos0 + dir * joint.distance.minDistance;
               auto posMax = pos0 + dir * joint.distance.maxDistance;
               dbgRend.DrawLine(posMin, posMax, defColor);
            } else if (joint.type == JointType::Revolute) {
               dbgRend.DrawLine(pos0, pos0 + trans0->Right() * 3.f, Color_Red);
               dbgRend.DrawSphere(sphere, defColor);
            } else if (joint.type == JointType::Spherical) {
               dbgRend.DrawSphere(sphere, defColor);
            } else if (joint.type == JointType::Prismatic) {
               dbgRend.DrawLine(
                  pos0 + trans0->Right() * joint.prismatic.lowerLimit,
                  pos0 + trans0->Right() * joint.prismatic.upperLimit,
                  defColor);
            } else {
               UNIMPLEMENTED();
            }
         }

         for (auto [_, rb] : scene.View<RigidBodyComponent>().each()) {
            rb.DbgRender(dbgRend);
         }

         dbgRend.Render(cmd, camera);
         dbgRend.Clear();
      }
   }

   void Renderer::RenderSceneAllObjects(CommandList& cmd, const std::vector<RenderObject>& renderObjs, GpuProgram& program) {
      program.Activate(cmd);
      program.SetSRV(cmd, "gInstances", *instanceBuffer);

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
         cb.instance.transform = glm::translate(mat4(1), trans.Position());
         cb.instance.transform = cb.instance.transform;
         cb.instance.material.roughness = material.roughness;
         cb.instance.material.baseColor = material.baseColor;
         cb.instance.material.metallic = material.metallic;
         cb.instance.entityID = (uint)trans.entity.GetID();
         cb.instanceStart = instanceID++;

         auto dynCB = cmd.AllocDynConstantBuffer(cb);
         program.SetCB<SDrawCallCB>(cmd, "gDrawCallCB", *dynCB.buffer, dynCB.offset);

         if (instancedDraw) {
            if (indirectDraw) {
               // DrawIndexedInstancedArgs args{ (uint)mesh.geom.IndexCount(), (uint)renderObjs.size(), 0, 0, 0 };
               DrawIndexedInstancedArgs args{ (uint)mesh.geom.IndexCount(), 0, 0, 0, 0 };
               auto dynArgs = cmd.AllocDynDrawIndexedInstancedBuffer(args, 1);

               if (1) {
                  auto indirectArgsTest = GetGpuProgram(ProgramDesc::Cs("cull.hlsl", "indirectArgsTest"));
               
                  indirectArgsTest->Activate(cmd);
               
                  indirectArgsTest->SetUAV(cmd, "gIndirectArgsInstanceCount", *dynArgs.buffer);

                  uint4 offset{ dynArgs.offset / sizeof(uint), (uint)renderObjs.size(), 0, 0};
                  auto testCB = cmd.AllocDynConstantBuffer(offset);
                  // todo: it removes base.hlsl cb
                  indirectArgsTest->SetCB<uint4>(cmd, "gTestCB", *testCB.buffer, testCB.offset);

                  cmd.Dispatch1D(1);
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

   void Renderer::RenderOutlines(CommandList& cmd, const Scene& scene) {
      auto programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "ps_main");
      programDesc.vs.defines.AddDefine("OUTLINES");
      programDesc.ps.defines.AddDefine("OUTLINES");

      auto program = *GetGpuProgram(programDesc);

      program.Activate(cmd);

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

      for (auto [_, trans, outline]
         : scene.View<SceneTransformComponent, OutlineComponent>().each()) {
         SDrawCallCB cb;
         cb.instance.transform = trans.GetWorldMatrix();
         cb.instance.material.baseColor = outline.color;
         cb.instance.entityID = (uint)trans.entity.GetID();

         auto dynCB = cmd.AllocDynConstantBuffer(cb);
         program.SetCB<SDrawCallCB>(cmd, "gDrawCallCB", *dynCB.buffer, dynCB.offset);

         program.DrawIndexedInstanced(cmd, mesh.geom.IndexCount());
      }
   }

   uint Renderer::GetEntityIDUnderCursor(CommandList& cmd) {
      uint entityID;
      cmd.ReadbackBuffer(*underCursorBuffer, 0, sizeof(uint), &entityID);

      return entityID;
   }

}
