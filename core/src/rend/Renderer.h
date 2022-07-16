#pragma once

#include "Buffer.h"
#include "Device.h"
#include "CommandList.h"
#include "GpuTimer.h"
#include "RendRes.h"
#include "Texture2D.h"
#include "Shader.h"
#include "math/Types.h"
#include "mesh/Mesh.h"
#include "scene/Component.h"
#include "scene/Scene.h"

#include "shared/common.hlsli"


namespace pbe {

   class Renderer {
   public:
      ~Renderer() {
         rendres::Term();
      }

      Ref<GpuProgram> baseColorPass;
      Ref<GpuProgram> baseZPass;

      Mesh mesh;

      Ref<Buffer> cameraCbBuffer;
      Ref<Buffer> instanceBuffer;
      Ref<Buffer> lightBuffer;
      int nLights = 4;

      GpuTimer timer;

      vec3 cameraPos{};
      float angle = 0;

      void Init() {
         rendres::Init(); // todo:

         auto programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "ps_main");
         baseColorPass = GpuProgram::Create(programDesc);

         programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main");
         programDesc.vs.defines.AddDefine("ZPASS");
         baseZPass = GpuProgram::Create(programDesc);

         mesh = Mesh::Create(MeshGeomCube());

         auto bufferDesc = Buffer::Desc::ConstantBuffer(sizeof(CameraCB));
         cameraCbBuffer = Buffer::Create(bufferDesc);
         cameraCbBuffer->SetDbgName("camera cb");

         bufferDesc = Buffer::Desc::StructureBuffer(nLights, sizeof(Light));
         lightBuffer = Buffer::Create(bufferDesc);
         lightBuffer->SetDbgName("light buffer");
      }

      void RenderDataPrepare(CommandList& cmd, Scene& scene) {
         if (!instanceBuffer || instanceBuffer->ElementsCount() < scene.EntitiesCount()) {
            auto bufferDesc = Buffer::Desc::StructureBuffer(scene.EntitiesCount(), sizeof(Instance));
            instanceBuffer = Buffer::Create(bufferDesc);
            instanceBuffer->SetDbgName("instance buffer");
         }

         std::vector<Instance> instances;
         instances.reserve(scene.EntitiesCount());
         for (auto [e, sceneTrans, material] : scene.GetEntitiesWith<SceneTransformComponent, SimpleMaterialComponent>().each()) {
            mat4 transform = glm::translate(mat4(1), sceneTrans.position);
            transform = glm::transpose(transform);

            instances.emplace_back(transform);
         }

         cmd.UpdateSubresource(*instanceBuffer, instances.data());

         std::vector<Light> lights;
         lights.resize(nLights); // 4

         lights[0].position = {};
         lights[0].color = vec3(2, 3, 1);

         lights[1].position = { 1, 2, 5 };
         lights[1].color = vec3(2, 30, 1);

         lights[2].position = { 10, 1, 5 };
         lights[2].color = vec3(2, 3, 10);

         lights[3].position = { -10, 2, 5 };
         lights[3].color = vec3(20, 3, 1);

         cmd.UpdateSubresource(*lightBuffer, lights.data());
      }

      void RenderScene(Texture2D& target, Texture2D& depth, CommandList& cmd, Scene& scene) {
         if (!baseColorPass->Valid() || !baseZPass->Valid()) {
            return;
         }

         GpuMarker marker{ cmd, "Render Scene" };
         timer.Start();

         RenderDataPrepare(cmd, scene);

         auto context = cmd.pContext;

         vec4 background_colour{0};
         cmd.ClearRenderTarget(target, background_colour);
         cmd.ClearDepthTarget(depth, 1);

         cmd.SetViewport({}, target.GetDesc().size);
         cmd.SetRasterizerState(rendres::rasterizerState);
         // context->OMSetBlendState(nullptr, nullptr, 0xffffffff);

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

         float3 direction = vec4(0, 0, 1, 1) * glm::rotate(mat4(1), glm::radians(angle), vec3_Up);

         mat4 view = glm::lookAt(cameraPos, cameraPos + direction, vec3_Y);
         mat4 proj = glm::perspectiveFov(90.f / (180) * pi, (float)target.GetDesc().size.x, (float)target.GetDesc().size.y, 0.1f, 100.f);

         cb.viewProjection = proj * view;
         cb.viewProjection = glm::transpose(cb.viewProjection);
         cb.position = cameraPos;
         cb.nLights = nLights;

         if (useZPass) {
            {
               GPU_EVENT("ZPass");

               cmd.SetRenderTargets(nullptr, &depth);
               cmd.SetDepthStencilState(rendres::depthStencilState);
               RenderSceneAllObjects(cmd, scene, *baseZPass, cb);
            }

            {
               GPU_EVENT("Color Pass");

               cmd.SetRenderTargets(&target, &depth);
               cmd.SetDepthStencilState(rendres::depthStencilStateEqual);
               RenderSceneAllObjects(cmd, scene, *baseColorPass, cb);
            }
         } else {
            GPU_EVENT("Color Pass (Without ZPass)");

            cmd.SetRenderTargets(&target, &depth);
            cmd.SetDepthStencilState(rendres::depthStencilState);
            RenderSceneAllObjects(cmd, scene, *baseColorPass, cb);
         }

         timer.Stop();
      }

      bool useZPass = false;

      void RenderSceneAllObjects(CommandList& cmd, Scene& scene, GpuProgram& program, const CameraCB& cameraCB) {
         CameraCB cb = cameraCB;

         program.Activate(cmd);
         program.SetConstantBuffer(cmd, "gCamera", *cameraCbBuffer);
         program.SetSrvBuffer(cmd, "gInstances", *instanceBuffer);
         program.SetSrvBuffer(cmd, "gLights", *lightBuffer);

         int instanceID = 0;

         for (auto [e, sceneTrans, material] : scene.GetEntitiesWith<SceneTransformComponent, SimpleMaterialComponent>().each()) {
            cb.transform = glm::translate(mat4(1), sceneTrans.position);
            cb.transform = glm::transpose(cb.transform);

            cb.color = material.albedo;
            cb.roughness = material.roughness;
            cb.metallic = material.metallic;
            cb.instanceStart = instanceID++;

            cmd.UpdateSubresource(*cameraCbBuffer, &cb);

            // baseColorPass->DrawInstanced(cmd, mesh.geom.VertexCount());
            program.DrawIndexedInstanced(cmd, mesh.geom.IndexCount());
         }
      }

   };

}
