#include "pch.h"
#include "Renderer.h"

#include "core/Profiler.h"
#include "math/Random.h"


namespace pbe {

   void Renderer::Init() {
      rendres::Init(); // todo:

      auto programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "ps_main");
      baseColorPass = GpuProgram::Create(programDesc);

      programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "ps_main");
      programDesc.vs.defines.AddDefine("ZPASS");
      programDesc.ps.defines.AddDefine("ZPASS");
      baseZPass = GpuProgram::Create(programDesc);

      programDesc = ProgramDesc::Cs("ssao.cs", "main");
      ssaoPass = GpuProgram::Create(programDesc);

      mesh = Mesh::Create(MeshGeomCube());

      auto bufferDesc = Buffer::Desc::ConstantBuffer(sizeof(CameraCB));
      cameraCbBuffer = Buffer::Create(bufferDesc);
      cameraCbBuffer->SetDbgName("camera cb");
   }

   void Renderer::UpdateInstanceBuffer(CommandList& cmd, const std::vector<RenderObject>& renderObjs) {
      if (!instanceBuffer || instanceBuffer->ElementsCount() < renderObjs.size()) {
         auto bufferDesc = Buffer::Desc::StructureBuffer((uint)renderObjs.size(), sizeof(Instance));
         instanceBuffer = Buffer::Create(bufferDesc);
         instanceBuffer->SetDbgName("instance buffer");
      }

      std::vector<Instance> instances;
      instances.reserve(renderObjs.size());
      for (auto& [trans, material] : renderObjs) {
         mat4 transform = glm::translate(mat4(1), trans.position);
         transform *= mat4{ trans.rotation };
         transform *= glm::scale(mat4(1), trans.scale);
         transform = glm::transpose(transform);

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
         auto bufferDesc = Buffer::Desc::StructureBuffer(scene.EntitiesCount(), sizeof(Instance));
         instanceBuffer = Buffer::Create(bufferDesc);
         instanceBuffer->SetDbgName("instance buffer");
      }

      {
         auto nLights = (int)scene.GetEntitiesWith<LightComponent>().size();
         if (!lightBuffer || lightBuffer->ElementsCount() < nLights) {
            auto bufferDesc = Buffer::Desc::StructureBuffer(nLights, sizeof(Light));
            lightBuffer = Buffer::Create(bufferDesc);
            lightBuffer->SetDbgName("light buffer");
         }

         std::vector<Light> lights;
         lights.reserve(nLights);

         for (auto [e, trans, light] : scene.GetEntitiesWith<SceneTransformComponent, LightComponent>().each()) {
            Light l;
            l.position = trans.position;
            l.color = light.color;

            lights.emplace_back(l);
         }

         cmd.UpdateSubresource(*lightBuffer, lights.data(), 0, lights.size() * sizeof(Light));
      }

      if (!ssaoRandomDirs) {
         int nRandomDirs = 32;
         auto bufferDesc = Buffer::Desc::StructureBuffer(nRandomDirs, sizeof(vec3));
         ssaoRandomDirs = Buffer::Create(bufferDesc);
         ssaoRandomDirs->SetDbgName("ssao random dirs");

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

      auto context = cmd.pContext;

      cmd.ClearRenderTarget(*cameraContext.color, vec4{0, 0, 0, 0});
      cmd.ClearRenderTarget(*cameraContext.normal, vec4{0, 0, 0, 0});
      cmd.ClearDepthTarget(*cameraContext.depth, 1);

      cmd.SetViewport({}, cameraContext.color->GetDesc().size);
      cmd.SetRasterizerState(rendres::rasterizerState);

      // set mesh
      context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      ID3D11InputLayout* inputLayout = rendres::GetInputLayout(baseColorPass->vs->blob.Get(), VertexPosNormal::inputElementDesc);
      context->IASetInputLayout(inputLayout);

      ID3D11Buffer* vBuffer = mesh.vertexBuffer->GetBuffer();
      uint offset = 0;
      context->IASetVertexBuffers(0, 1, &vBuffer, &mesh.geom.nVertexByteSize, &offset);
      context->IASetIndexBuffer(mesh.indexBuffer->GetBuffer(), DXGI_FORMAT_R16_UINT, 0);
      //

      CameraCB cb;

      cb.view = glm::transpose(camera.view);
      cb.projection = glm::transpose(camera.projection);
      cb.viewProjection = glm::transpose(camera.GetViewProjection());
      cb.invViewProjection = glm::inverse(cb.viewProjection);
      cb.rtSize = cameraContext.color->GetDesc().size;
      cb.position = camera.position;
      cb.nLights = (int)scene.GetEntitiesWith<LightComponent>().size();
      cb.directLight.color = vec3{ 1 };
      cb.directLight.direction = glm::normalize(vec3{ -1, -1, -1 });

      cmd.SetBlendState(nullptr);

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

      if (cfg.useZPass) {
         {
            GPU_MARKER("ZPass");
            PROFILE_GPU("ZPass");

            cmd.SetRenderTargets(cameraContext.normal, cameraContext.depth);
            cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);

            RenderSceneAllObjects(cmd, opaqueObjs, *baseZPass, cb);
         }

         if (cfg.ssao) {
            GPU_MARKER("SSAO");
            PROFILE_GPU("SSAO");

            cmd.SetRenderTargets();

            ssaoPass->Activate(cmd);
            ssaoPass->SetCB(cmd, "gCameraCB", *cameraCbBuffer);
            cmd.pContext->CSSetSamplers(0, 1, &rendres::samplerStatePoint); // todo:
            ssaoPass->SetSRV(cmd, "gDepth", *cameraContext.depth);
            ssaoPass->SetSRV(cmd, "gRandomDirs", *ssaoRandomDirs);
            ssaoPass->SetSRV(cmd, "gNormal", *cameraContext.normal);
            ssaoPass->SetUAV(cmd, "gSsao", *cameraContext.ssao);

            ssaoPass->Dispatch(cmd, glm::ceil(vec2{cameraContext.color->GetDesc().size} / vec2{ 8 }));

            // todo:
            ID3D11ShaderResourceView* views[] = {nullptr};
            cmd.pContext->CSSetShaderResources(0, 1, views);

            ID3D11UnorderedAccessView* viewsUAV[] = { nullptr };
            cmd.pContext->CSSetUnorderedAccessViews(0, 1, viewsUAV, nullptr);
         }

         {
            GPU_MARKER("Color");
            PROFILE_GPU("Color");

            cmd.SetRenderTargets(cameraContext.color, cameraContext.depth);
            cmd.SetDepthStencilState(rendres::depthStencilStateEqual);
            cmd.pContext->PSSetSamplers(0, 1, &rendres::samplerStateLinear); // todo:
            baseColorPass->SetSRV(cmd, "gSsao", *cameraContext.ssao);
            RenderSceneAllObjects(cmd, opaqueObjs, *baseColorPass, cb);
         }
      } else {
         GPU_MARKER("Color (Without ZPass)");
         PROFILE_GPU("Color (Without ZPass)");

         cmd.SetRenderTargets(cameraContext.color, cameraContext.depth);
         cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);
         baseColorPass->SetSRV(cmd, "gSsao", *cameraContext.ssao);

         RenderSceneAllObjects(cmd, opaqueObjs, *baseColorPass, cb);
      }

      if (cfg.renderTransparency) {
         GPU_MARKER("Transparency");
         PROFILE_GPU("Transparency");

         cmd.SetRenderTargets(cameraContext.color, cameraContext.depth);
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
         RenderSceneAllObjects(cmd, transparentObjs, *baseColorPass, cb);
      }
   }

   void Renderer::RenderSceneAllObjects(CommandList& cmd, const std::vector<RenderObject>& renderObjs,
      GpuProgram& program, const CameraCB& cameraCB) {
      CameraCB cb = cameraCB;

      program.Activate(cmd);
      program.SetCB(cmd, "gCameraCB", *cameraCbBuffer);
      program.SetSRV(cmd, "gInstances", *instanceBuffer);
      program.SetSRV(cmd, "gLights", *lightBuffer);

      int instanceID = 0;

      for (const auto& [trans, material] : renderObjs) {
         cb.transform = glm::translate(mat4(1), trans.position);
         cb.transform = glm::transpose(cb.transform);

         cb.instanceStart = instanceID++;

         cmd.UpdateSubresource(*cameraCbBuffer, &cb);

         if (cfg.useInstancedDraw) {
            program.DrawIndexedInstanced(cmd, mesh.geom.IndexCount(), (int)renderObjs.size());
            break;
         } else {
            // program->DrawInstanced(cmd, mesh.geom.VertexCount());
            program.DrawIndexedInstanced(cmd, mesh.geom.IndexCount());
         }
      }
   }

}
