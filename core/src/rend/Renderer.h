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

      Ref<GpuProgram> program;

      Mesh mesh;

      Ref<Buffer> cameraCbBuffer;

      GpuTimer timer;

      vec3 cameraPos{};
      float angle = 0;

      void Init() {
         rendres::Init(); // todo:

         auto programDesc = ProgramDesc::VsPs("shaders.hlsl", "vs_main", "ps_main");
         program = GpuProgram::Create(programDesc);

         mesh = Mesh::Create(MeshGeomCube());

         auto bufferDesc = Buffer::Desc::ConstantBuffer(sizeof(CameraCB));
         cameraCbBuffer = Buffer::Create(bufferDesc);
         cameraCbBuffer->SetDbgName("camera cb");
      }

      void RenderScene(Texture2D& target, Texture2D& depth, CommandList& cmd, Scene& scene) {
         GpuMarker marker{ cmd, "Color Pass" };

         auto context = cmd.pContext;

         vec4 background_colour{0};
         cmd.ClearRenderTarget(target, background_colour);
         cmd.ClearDepthTarget(depth, 1);

         cmd.SetRenderTargets(&target, &depth);
         cmd.SetViewport({}, target.GetDesc().size);
         cmd.SetDepthStencilState(rendres::depthStencilState);
         cmd.SetRasterizerState(rendres::rasterizerState);
         // context->OMSetBlendState(nullptr, nullptr, 0xffffffff);

         // set mesh
         context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

         ID3D11InputLayout* inputLayout = rendres::GetInputLayout(program->vs->blob.Get(), VertexPosNormal::inputElementDesc);
         context->IASetInputLayout(inputLayout);

         ID3D11Buffer* vBuffer = mesh.vertexBuffer->GetBuffer();
         uint offset = 0;
         context->IASetVertexBuffers(0, 1, &vBuffer, &mesh.geom.nVertexByteSize, &offset);
         context->IASetIndexBuffer(mesh.indexBuffer->GetBuffer(), DXGI_FORMAT_R16_UINT, 0);
         //

         program->Activate(cmd);
         program->SetConstantBuffer(cmd, "gCamera", *cameraCbBuffer);

         CameraCB cb;

         float3 direction = vec4(0, 0, 1, 1) * glm::rotate(mat4(1), glm::radians(angle), vec3_Up);

         mat4 view = glm::lookAt(cameraPos, cameraPos + direction, vec3_Y);
         mat4 proj = glm::perspectiveFov(90.f / (180) * pi, (float)target.GetDesc().size.x, (float)target.GetDesc().size.y, 0.1f, 100.f);

         cb.viewProjection = proj * view;
         cb.viewProjection = glm::transpose(cb.viewProjection);

         timer.Start();

         for (auto [e, sceneTrans, material] : scene.GetEntitiesWith<SceneTransformComponent, SimpleMaterialComponent>().each()) {
            cb.transform = glm::translate(mat4(1), sceneTrans.position);
            cb.transform = glm::transpose(cb.transform);

            cb.color = material.albedo;

            context->UpdateSubresource(cameraCbBuffer->GetBuffer(), 0, nullptr, &cb, 0, 0);

            // program->DrawInstanced(cmd, mesh.geom.VertexCount());
            program->DrawIndexedInstanced(cmd, mesh.geom.IndexCount());
         }

         timer.Stop();
      }

   };

}
