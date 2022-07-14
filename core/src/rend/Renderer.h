#pragma once

#include "Buffer.h"
#include "Device.h"
#include "CommandList.h"
#include "RendRes.h"
#include "Texture2D.h"
#include "Shader.h"
#include "core/Log.h"
#include "math/Types.h"
#include "scene/Component.h"
#include "scene/Scene.h"


namespace pbe {

   struct CameraCB {
      float4x4 viewProjection;
      float4x4 transform;
      float3 color;
      float _dymmy;
   };

   struct VertexPos {
      float3 position;

      static std::vector<D3D11_INPUT_ELEMENT_DESC> inputElementDesc;
   };

   struct VertexPosNormal {
      float3 position;
      float3 normal;

      static std::vector<D3D11_INPUT_ELEMENT_DESC> inputElementDesc;
   };

   class Renderer {
   public:
      ~Renderer() {
         INFO("Renderer destroy");

         rendres::Term();
      }

      ComPtr<ID3D11InputLayout> input_layout_ptr;

      Ref<GpuProgram> program;
      Ref<Buffer> vertexBuffer;
      Ref<Buffer> cameraCbBuffer;

      UINT vertex_stride{};
      UINT vertex_offset{};
      UINT vertex_count{};

      vec3 cameraPos{};
      float angle = 0;

      void Init() {
         rendres::Init(); // todo:

         // VertexPos::inputElementDesc = {
         //    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
         //    /*
         //    { "COL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
         //    { "NOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
         //    { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
         //    */
         // };
         //
         // VertexPosNormal::inputElementDesc = {
         //    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
         //    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
         // };

         auto* device_ptr = sDevice->g_pd3dDevice;

         auto programDesc = ProgramDesc::VsPs("shaders.hlsl", "vs_main", "ps_main");
         program = GpuProgram::Create(programDesc);

         HRESULT hr;

         D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
            {"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            /*
            { "COL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            */
         };

         ID3DBlob* vs_blob_ptr = program->vs->blob;

         hr = device_ptr->CreateInputLayout(
            inputElementDesc,
            ARRAYSIZE(inputElementDesc),
            vs_blob_ptr->GetBufferPointer(),
            vs_blob_ptr->GetBufferSize(),
            input_layout_ptr.GetAddressOf());
         assert(SUCCEEDED(hr));
         SetDbgName(input_layout_ptr.Get(), "triangle input layout");

         float vertex_data_array[] = {
            0.0f, 0.5f, 0.0f, // point at top
            0.5f, -0.5f, 0.0f, // point at bottom-right
            -0.5f, -0.5f, 0.0f, // point at bottom-left
         };
         vertex_stride = 3 * sizeof(float);
         vertex_offset = 0;
         vertex_count = 3;

         {
            auto bufferDesc = Buffer::Desc::VertexBuffer(sizeof(vertex_data_array));
            vertexBuffer = Buffer::Create(bufferDesc, vertex_data_array);
            vertexBuffer->SetDbgName("triangle vert buf");
         }

         {
            auto bufferDesc = Buffer::Desc::ConstantBuffer(sizeof(CameraCB));
            cameraCbBuffer = Buffer::Create(bufferDesc);
            cameraCbBuffer->SetDbgName("camera cb");
         }
      }

      void RenderScene(Texture2D& target, Texture2D& depth, CommandList& cmd, Scene& scene) {
         GpuMarker marker{ cmd, "Color Pass" };

         auto context = cmd.pContext;

         // vec4 background_colour{0x64 / 255.0f, 0x95 / 255.0f, 0xED / 255.0f, 1.0f};
         vec4 background_colour{0};
         cmd.ClearRenderTarget(target, background_colour);
         cmd.ClearDepthTarget(depth, 1);

         cmd.SetRenderTargets(&target, &depth);
         cmd.SetViewport({}, target.GetDesc().size);
         cmd.SetDepthStencilState(rendres::depthStencilState);

         /***** Input Assembler (map how the vertex shader inputs should be read from vertex buffer) ******/
         context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
         context->IASetInputLayout(input_layout_ptr.Get());

         ID3D11Buffer* vBuffer = vertexBuffer->GetBuffer();
         context->IASetVertexBuffers(0, 1, &vBuffer, &vertex_stride, &vertex_offset);

         program->Activate(cmd);
         program->SetConstantBuffer(cmd, "gCamera", *cameraCbBuffer);

         CameraCB cb;

         float3 direction = vec4(0, 0, 1, 1) * glm::rotate(mat4(1), angle / 180.f * pi, vec3_Up);

         mat4 view = glm::lookAt(cameraPos, cameraPos + direction, vec3_Y);
         mat4 proj = glm::perspectiveFov(90.f / (180) * pi, (float)target.GetDesc().size.x, (float)target.GetDesc().size.y, 0.1f, 100.f);

         cb.viewProjection = proj * view;
         cb.viewProjection = glm::transpose(cb.viewProjection);

         for (auto [e, sceneTrans, material] : scene.GetEntitiesWith<SceneTransformComponent, SimpleMaterialComponent>().each()) {
            cb.transform = glm::translate(mat4(1), sceneTrans.position);
            cb.transform = glm::transpose(cb.transform);

            cb.color = material.albedo;

            context->UpdateSubresource(cameraCbBuffer->GetBuffer(), 0, nullptr, &cb, 0, 0);

            program->DrawInstanced(cmd, vertex_count);
         }

         // D3D11_MAPPED_SUBRESOURCE mappedSubresource;
         //
         // deviceContext->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
         //
         // Constants* constants = reinterpret_cast<Constants*>(mappedSubresource.pData);
         //
         // deviceContext->Unmap(constantBuffer, 0);
         //
         // ///////////////////////////////////////////////////////////////////////////////////////////
         //
         // deviceContext->ClearRenderTargetView(frameBufferView, backgroundColor);
         // deviceContext->ClearDepthStencilView(depthBufferView, D3D11_CLEAR_DEPTH, 1.0f, 0);
         //
         // deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
         // deviceContext->IASetInputLayout(inputLayout);
         // deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
         // deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
         //
         // deviceContext->VSSetShader(vertexShader, nullptr, 0);
         // deviceContext->VSSetConstantBuffers(0, 1, &constantBuffer);
         //
         // deviceContext->RSSetViewports(1, &viewport);
         // deviceContext->RSSetState(rasterizerState);
         //
         // deviceContext->PSSetShader(pixelShader, nullptr, 0);
         // deviceContext->PSSetShaderResources(0, 1, &textureView);
         // deviceContext->PSSetSamplers(0, 1, &samplerState);
         //
         // deviceContext->OMSetRenderTargets(1, &frameBufferView, depthBufferView);
         // deviceContext->OMSetDepthStencilState(depthStencilState, 0);
         // deviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff); // use default blend mode (i.e. disable)
         //
         // ///////////////////////////////////////////////////////////////////////////////////////////
         //
         // deviceContext->DrawIndexed(ARRAYSIZE(IndexData), 0, 0);
      }

   };

}
