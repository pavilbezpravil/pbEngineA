#pragma once

#include "Buffer.h"
#include "Device.h"
#include "CommandList.h"
#include "Texture2D.h"

#include "Shader.h"
#include "core/Log.h"
#include "math/Types.h"


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
      }

      ComPtr<ID3D11InputLayout> input_layout_ptr;

      Ref<GpuProgram> program;
      Ref<Buffer> vertexBuffer;
      Ref<Buffer> cameraCbBuffer;

      UINT vertex_stride{};
      UINT vertex_offset{};
      UINT vertex_count{};

      vec3 triangleTranslate = vec3_Z;
      vec3 cameraPos{};
      vec3 triangleColor{};
      float angle = 0;

      void Init() {
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

         // auto* device = sDevice->g_pd3dDevice;
         //
         // ///////////////////////////////////////////////////////////////////////////////////////////////
         //
         // D3D11_TEXTURE2D_DESC depthBufferDesc;
         //
         // frameBuffer->GetDesc(&depthBufferDesc); // base on framebuffer properties
         //
         // depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
         // depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
         //
         // ID3D11Texture2D* depthBuffer;
         //
         // device->CreateTexture2D(&depthBufferDesc, nullptr, &depthBuffer);
         //
         // ID3D11DepthStencilView* depthBufferView;
         //
         // device->CreateDepthStencilView(depthBuffer, nullptr, &depthBufferView);
         //
         // ///////////////////////////////////////////////////////////////////////////////////////////////
         //
         // ID3DBlob* vsBlob;
         //
         // D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &vsBlob, nullptr);
         //
         // ID3D11VertexShader* vertexShader;
         //
         // device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
         //
         // D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = // float3 position, float3 normal, float2 texcoord, float3 color
         // {
         //     { "POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,                            0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
         //     { "NOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
         //     { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
         //     { "COL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
         // };
         //
         // ID3D11InputLayout* inputLayout;
         //
         // device->CreateInputLayout(inputElementDesc, ARRAYSIZE(inputElementDesc), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
         //
         // ///////////////////////////////////////////////////////////////////////////////////////////////
         //
         // ID3DBlob* psBlob;
         //
         // D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &psBlob, nullptr);
         //
         // ID3D11PixelShader* pixelShader;
         //
         // device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);
         //
         // ///////////////////////////////////////////////////////////////////////////////////////////////
         //
         // D3D11_RASTERIZER_DESC1 rasterizerDesc = {};
         // rasterizerDesc.FillMode = D3D11_FILL_SOLID;
         // rasterizerDesc.CullMode = D3D11_CULL_BACK;
         //
         // ID3D11RasterizerState1* rasterizerState;
         //
         // device->CreateRasterizerState1(&rasterizerDesc, &rasterizerState);
         //
         // ///////////////////////////////////////////////////////////////////////////////////////////////
         //
         // D3D11_SAMPLER_DESC samplerDesc = {};
         // samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
         // samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
         // samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
         // samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
         // samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
         //
         // ID3D11SamplerState* samplerState;
         //
         // device->CreateSamplerState(&samplerDesc, &samplerState);
         //
         // ///////////////////////////////////////////////////////////////////////////////////////////////
         //
         // D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
         // depthStencilDesc.DepthEnable = TRUE;
         // depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
         // depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
         //
         // ID3D11DepthStencilState* depthStencilState;
         //
         // device->CreateDepthStencilState(&depthStencilDesc, &depthStencilState);
         //
         // ///////////////////////////////////////////////////////////////////////////////////////////////
         //
         // struct Constants
         // {
         //    matrix Transform;
         //    matrix Projection;
         //    float3 LightVector;
         // };
         //
         // D3D11_BUFFER_DESC constantBufferDesc = {};
         // constantBufferDesc.ByteWidth = sizeof(Constants) + 0xf & 0xfffffff0; // round constant buffer size to 16 byte boundary
         // constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
         // constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
         // constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
         //
         // ID3D11Buffer* constantBuffer;
         //
         // device->CreateBuffer(&constantBufferDesc, nullptr, &constantBuffer);
         //
         // ///////////////////////////////////////////////////////////////////////////////////////////////
         //
         // D3D11_BUFFER_DESC vertexBufferDesc = {};
         // vertexBufferDesc.ByteWidth = sizeof(VertexData);
         // vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
         // vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
         //
         // D3D11_SUBRESOURCE_DATA vertexData = { VertexData };
         //
         // ID3D11Buffer* vertexBuffer;
         //
         // device->CreateBuffer(&vertexBufferDesc, &vertexData, &vertexBuffer);
         //
         // ///////////////////////////////////////////////////////////////////////////////////////////////
         //
         // D3D11_BUFFER_DESC indexBufferDesc = {};
         // indexBufferDesc.ByteWidth = sizeof(IndexData);
         // indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
         // indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
         //
         // D3D11_SUBRESOURCE_DATA indexData = { IndexData };
         //
         // ID3D11Buffer* indexBuffer;
         //
         // device->CreateBuffer(&indexBufferDesc, &indexData, &indexBuffer);
         //
         // ///////////////////////////////////////////////////////////////////////////////////////////////
         //
         // D3D11_TEXTURE2D_DESC textureDesc = {};
         // textureDesc.Width = TEXTURE_WIDTH;  // in data.h
         // textureDesc.Height = TEXTURE_HEIGHT; // in data.h
         // textureDesc.MipLevels = 1;
         // textureDesc.ArraySize = 1;
         // textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
         // textureDesc.SampleDesc.Count = 1;
         // textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
         // textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
         //
         // D3D11_SUBRESOURCE_DATA textureData = {};
         // textureData.pSysMem = TextureData;
         // textureData.SysMemPitch = TEXTURE_WIDTH * 4; // 4 bytes per pixel
         //
         // ID3D11Texture2D* texture;
         //
         // device->CreateTexture2D(&textureDesc, &textureData, &texture);
         //
         // ID3D11ShaderResourceView* textureView;
         //
         // device->CreateShaderResourceView(texture, nullptr, &textureView);
         //
         // ///////////////////////////////////////////////////////////////////////////////////////////////
         //
         // FLOAT backgroundColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
         //
         // UINT stride = 11 * 4; // vertex size (11 floats: float3 position, float3 normal, float2 texcoord, float3 color)
         // UINT offset = 0;
         //
         // D3D11_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(depthBufferDesc.Width), static_cast<float>(depthBufferDesc.Height), 0.0f, 1.0f };
         //
         // ///////////////////////////////////////////////////////////////////////////////////////////////
         //
         // float w = viewport.Width / viewport.Height; // width (aspect ratio)
         // float h = 1.0f;                             // height
         // float n = 1.0f;                             // near
         // float f = 9.0f;                             // far
         //
         // float3 modelRotation = { 0.0f, 0.0f, 0.0f };
         // float3 modelScale = { 1.0f, 1.0f, 1.0f };
         // float3 modelTranslation = { 0.0f, 0.0f, 4.0f };

      }

      void Render(Texture2D& target, CommandList& cmd) {
         auto context = cmd.pContext;

         /* clear the back buffer to cornflower blue for the new frame */
         vec4 background_colour{0x64 / 255.0f, 0x95 / 255.0f, 0xED / 255.0f, 1.0f};
         cmd.ClearRenderTarget(target, background_colour);

         D3D11_VIEWPORT viewport = {
            0.0f, 0.0f, (FLOAT)target.GetDesc().size.x, (FLOAT)target.GetDesc().size.y, 0.0f, 1.0f
         };
         context->RSSetViewports(1, &viewport);

         cmd.SetRenderTargets(&target);

         /***** Input Assembler (map how the vertex shader inputs should be read from vertex buffer) ******/
         context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
         context->IASetInputLayout(input_layout_ptr.Get());

         CameraCB cb;

         float3 direction = vec4(0, 0, 1, 1) * glm::rotate(mat4(1), angle / 180.f * pi, vec3_Up);

         mat4 view = glm::lookAt(cameraPos, cameraPos + direction, vec3_Y);
         mat4 proj = glm::perspectiveFov(90.f / (180) * pi, (float)target.GetDesc().size.x, (float)target.GetDesc().size.y, 0.1f, 100.f);

         cb.viewProjection = proj * view;
         cb.transform = glm::translate(mat4(1), triangleTranslate);
         cb.color = triangleColor;

         cb.viewProjection = glm::transpose(cb.viewProjection);
         cb.transform = glm::transpose(cb.transform);

         context->UpdateSubresource(cameraCbBuffer->GetBuffer(), 0, nullptr, &cb, 0, 0);

         ID3D11Buffer* vBuffer = vertexBuffer->GetBuffer();
         context->IASetVertexBuffers(0, 1, &vBuffer, &vertex_stride, &vertex_offset);

         program->Activate(cmd);
         program->SetConstantBuffer(cmd, "gCamera", *cameraCbBuffer);
         program->DrawInstanced(cmd, vertex_count);


         // matrix rotateX = { 1, 0, 0, 0, 0, static_cast<float>(cos(modelRotation.x)), -static_cast<float>(sin(modelRotation.x)), 0, 0, static_cast<float>(sin(modelRotation.x)), static_cast<float>(cos(modelRotation.x)), 0, 0, 0, 0, 1 };
         // matrix rotateY = { static_cast<float>(cos(modelRotation.y)), 0, static_cast<float>(sin(modelRotation.y)), 0, 0, 1, 0, 0, -static_cast<float>(sin(modelRotation.y)), 0, static_cast<float>(cos(modelRotation.y)), 0, 0, 0, 0, 1 };
         // matrix rotateZ = { static_cast<float>(cos(modelRotation.z)), -static_cast<float>(sin(modelRotation.z)), 0, 0, static_cast<float>(sin(modelRotation.z)), static_cast<float>(cos(modelRotation.z)), 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
         // matrix scale = { modelScale.x, 0, 0, 0, 0, modelScale.y, 0, 0, 0, 0, modelScale.z, 0, 0, 0, 0, 1 };
         // matrix translate = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, modelTranslation.x, modelTranslation.y, modelTranslation.z, 1 };
         //
         // modelRotation.x += 0.005f;
         // modelRotation.y += 0.009f;
         // modelRotation.z += 0.001f;
         //
         // ///////////////////////////////////////////////////////////////////////////////////////////
         //
         // D3D11_MAPPED_SUBRESOURCE mappedSubresource;
         //
         // deviceContext->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
         //
         // Constants* constants = reinterpret_cast<Constants*>(mappedSubresource.pData);
         //
         // constants->Transform = rotateX * rotateY * rotateZ * scale * translate;
         // constants->Projection = { 2 * n / w, 0, 0, 0, 0, 2 * n / h, 0, 0, 0, 0, f / (f - n), 1, 0, 0, n * f / (n - f), 0 };
         // constants->LightVector = { 1.0f, -1.0f, 1.0f };
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
