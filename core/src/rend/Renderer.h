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

#include "shared/common.hlsli"


namespace pbe {

   struct VertexPos {
      vec3 position;

      static inline std::vector<D3D11_INPUT_ELEMENT_DESC> inputElementDesc;
   };

   struct VertexPosNormal {
      vec3 position;
      vec3 normal;

      static inline std::vector<D3D11_INPUT_ELEMENT_DESC> inputElementDesc;
   };

   class MeshGeom {
   public:
      std::vector<byte> vertexes;
      std::vector<int16> indexes;

      uint nVertexByteSize = 0;

      MeshGeom() = default;
      MeshGeom(int nVertexByteSize) : nVertexByteSize(nVertexByteSize) {}

      int VertexesBytes() const { return (int)vertexes.size(); }
      int IndexesBytes() const { return IndexCount() * sizeof(uint16); }
      int VertexCount() const { return int(vertexes.size() / nVertexByteSize); }
      int IndexCount() const { return (int)indexes.size(); }

      // template<typename T>
      // void SetVertex(int iVert, T&& v) {
      //    vertexes[sizeof(T) * iVert] = std::move(v);
      // }

      template<typename T>
      std::vector<T>& VertexesAs() {
         return *(std::vector<T>*)&vertexes;
      }
   };

   class Mesh {
   public:
      Mesh() = default;
      Mesh(MeshGeom&& geom) : geom(std::move(geom)) {}

      Ref<Buffer> vertexBuffer;
      Ref<Buffer> indexBuffer;

      MeshGeom geom;

      static Mesh Create(MeshGeom&& geom) {
         Mesh mesh{ std::move(geom) };

         auto bufferDesc = Buffer::Desc::VertexBuffer(mesh.geom.VertexesBytes());
         mesh.vertexBuffer = Buffer::Create(bufferDesc, mesh.geom.vertexes.data());
         mesh.vertexBuffer->SetDbgName("vertex buf");

         bufferDesc = Buffer::Desc::IndexBuffer(mesh.geom.IndexesBytes());
         mesh.indexBuffer = Buffer::Create(bufferDesc, mesh.geom.indexes.data());
         mesh.indexBuffer->SetDbgName("index buf");

         return mesh;
      }
   };

   class GpuTimer {
   public:
      D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData{};
      uint64 start = 0;
      uint64 stop = 0;

      bool busy = false;
      float time{};

      ComPtr<ID3D11Query> disjointQuery;
      ComPtr<ID3D11Query> startQuery;
      ComPtr<ID3D11Query> stopQuery;

      GpuTimer() {
         auto device = sDevice->g_pd3dDevice;

         CD3D11_QUERY_DESC queryDesc(D3D11_QUERY_TIMESTAMP);
         device->CreateQuery(&queryDesc, startQuery.GetAddressOf());
         device->CreateQuery(&queryDesc, stopQuery.GetAddressOf());

         queryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
         device->CreateQuery(&queryDesc, disjointQuery.GetAddressOf());
      }

      void Start() {
         if (busy) {
            return;
         }

         auto context = sDevice->g_pd3dDeviceContext;
         context->Begin(disjointQuery.Get());
         context->End(startQuery.Get());
      }

      void Stop() {
         if (busy) {
            return;
         }
         busy = true;

         auto context = sDevice->g_pd3dDeviceContext;
         context->End(stopQuery.Get());
         context->End(disjointQuery.Get());
      }

      bool Ready() {
         auto context = sDevice->g_pd3dDeviceContext;

         if (context->GetData(disjointQuery.Get(), &disjointData, sizeof(disjointData), 0) == S_OK) {
            if (disjointData.Disjoint) {
               INFO("Disjoint!");
               return false;
            }
         } else {
            return false;
         }

         if (context->GetData(startQuery.Get(), &start, sizeof(start), 0) == S_OK) {
            // INFO("Start!");
         } else {
            return false;
         }

         if (context->GetData(stopQuery.Get(), &stop, sizeof(stop), 0) == S_OK) {
            // INFO("Stop!");
         } else {
            return false;
         }

         time = float(double(stop - start) / double(disjointData.Frequency) * 1000.);

         busy = false;
         return true;
      }

      float GetTimeMs() {
         Ready();
         return time;
      }
   };

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
         VertexPos s[] = {
            {{1.f, 1.f, 1.f}},
            {{1.f, 1.f, 1.f}},
         };

         rendres::Init(); // todo:

         VertexPos::inputElementDesc = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
         };
         
         VertexPosNormal::inputElementDesc = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
         };

         auto programDesc = ProgramDesc::VsPs("shaders.hlsl", "vs_main", "ps_main");
         program = GpuProgram::Create(programDesc);

         MeshGeom cube{sizeof(VertexPosNormal)};

         // todo: super dirty
         cube.vertexes.resize(sizeof(VertexPosNormal) * 8);

         // auto a = cube.VertexesAs<VertexPosNormal>();
         // a =
         cube.VertexesAs<VertexPosNormal>() =
         {
             {{ -1.0f, 1.0f, -1.0f}, {0, 0, 255}, },
             {{ 1.0f, 1.0f, -1.0f}, {0, 255, 0}, },
             {{ -1.0f, -1.0f, -1.0f}, {255, 0, 0}, },
             {{ 1.0f, -1.0f, -1.0f}, {0, 255, 255}, },
             {{ -1.0f, 1.0f, 1.0f}, {0, 0, 255}, },
             {{ 1.0f, 1.0f, 1.0f}, {255, 0, 0}, },
             {{ -1.0f, -1.0f, 1.0f}, {0, 255, 0}, },
             {{ 1.0f, -1.0f, 1.0f}, {0, 255, 255}, },
         };

         cube.indexes = {
              0, 1, 2,    // side 1
              2, 1, 3,
              4, 0, 6,    // side 2
              6, 0, 2,
              7, 5, 6,    // side 3
              6, 5, 4,
              3, 1, 7,    // side 4
              7, 1, 5,
              4, 5, 0,    // side 5
              0, 5, 1,
              3, 7, 2,    // side 6
              2, 7, 6,
         };

         mesh = Mesh::Create(std::move(cube));

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

         float3 direction = vec4(0, 0, 1, 1) * glm::rotate(mat4(1), angle / 180.f * pi, vec3_Up);

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
