#pragma once
// todo:
#include "Buffer.h"
#include "Device.h"
#include "Texture2D.h"
#include "core/Core.h"

namespace pbe {

   struct OffsetedBuffer {
      Buffer* buffer{};
      uint offset = 0;
   };

   class CORE_API CommandList {
   public:
      static constexpr int DYN_CONST_BUFFER_SIZE = 256 * 512;
      static constexpr int DYN_VERT_BUFFER_SIZE = 256 * 512;

      CommandList(ID3D11DeviceContext3* pContext) : pContext(pContext) {

      }

      // CommandList() {
      //    sDevice->g_pd3dDevice->CreateDeferredContext(0, &pContext);
      // }

      template<typename T>
      OffsetedBuffer AllocDynConstantBuffer(const T& data) {
         return AllocDynConstantBuffer((const void*) & data, sizeof(T));
      }

      OffsetedBuffer AllocDynConstantBuffer(const void* data, uint size) {
         size = ((size - 1) / 256 + 1) * 256; // todo:

         if (dynConstBuffers.empty() || dynConstBufferOffset + size >= DYN_CONST_BUFFER_SIZE) {
            auto bufferDesc = Buffer::Desc::Constant("cmdList dynConstBuffer", DYN_CONST_BUFFER_SIZE);
            dynConstBuffers.emplace_back(Buffer::Create(bufferDesc));

            dynConstBufferOffset = 0;
         }

         uint oldOffset = dynConstBufferOffset;
         dynConstBufferOffset += size;

         auto& curDynConstBuffer = *dynConstBuffers.back();

         UpdateSubresource(curDynConstBuffer, data, oldOffset, size);

         return { &curDynConstBuffer, oldOffset };
      }

      OffsetedBuffer AllocDynVertBuffer(const void* data, uint size) {
         size = ((size - 1) / 256 + 1) * 256; // todo:

         if (dynVertBuffers.empty() || dynVertBufferOffset + size >= DYN_VERT_BUFFER_SIZE) {
            auto bufferDesc = Buffer::Desc::Vertex("cmdList dynVertBuffer", DYN_VERT_BUFFER_SIZE);
            dynVertBuffers.emplace_back(Buffer::Create(bufferDesc));

            dynVertBufferOffset = 0;
         }

         uint oldOffset = dynVertBufferOffset;
         dynVertBufferOffset += size;

         auto& curDynConstBuffer = *dynVertBuffers.back();

         UpdateSubresource(curDynConstBuffer, data, oldOffset, size);

         return { &curDynConstBuffer, oldOffset };
      }

      template<typename T>
      void UpdateSubresource(Buffer& buffer, const T& data, uint offset = 0) {
         UpdateSubresource(buffer, (const void*)&data, offset, sizeof(T));
      }

      void UpdateSubresource(Buffer& buffer, const void* data, uint offset = 0, size_t size = -1) {
         if (buffer.Valid() && size > 0) {
            D3D11_BOX box{};
            box.left = offset;
            box.right = offset + (uint)size;
            box.bottom = 1;
            box.back = 1;

            pContext->UpdateSubresource1(buffer.GetBuffer(), 0,
               size == -1 ? nullptr : &box, data, 0, 0, D3D11_COPY_NO_OVERWRITE);
         }
      }

      void CopyResource(GPUResource& dst, GPUResource& src) {
         pContext->CopyResource(dst.pResource.Get(), src.pResource.Get());
      }

      void ClearDepthTarget(Texture2D& depth, float depthValue, uint8 stencilValue = 0, uint clearFlags = D3D11_CLEAR_DEPTH) {
         pContext->ClearDepthStencilView(depth.dsv.Get(), clearFlags, depthValue, stencilValue);
      }

      void ClearRenderTarget(Texture2D& rt, const vec4& color) {
         pContext->ClearRenderTargetView(rt.rtv.Get(), &color.x);
      }

      void SetRenderTargets(Texture2D* rt = nullptr, Texture2D* depth = nullptr) {
         int nRtvs = rt ? 1 : 0;
         ID3D11RenderTargetView** rtv = rt ? rt->rtv.GetAddressOf() : nullptr;
         ID3D11DepthStencilView* dsv = depth ? depth->dsv.Get() : nullptr;

         pContext->OMSetRenderTargets(nRtvs, rtv, dsv);
      }

      void SetDepthStencilState(ID3D11DepthStencilState* state, uint stencilRef = 0) {
         pContext->OMSetDepthStencilState(state, stencilRef);
      }

      void SetBlendState(ID3D11BlendState* state, const vec4& blendFactor = vec4{1.f}) {
         pContext->OMSetBlendState(state, &blendFactor.x, 0xffffffff);
      }

      void SetRasterizerState(ID3D11RasterizerState1* state) {
         pContext->RSSetState(state);
      }

      void SetViewport(vec2 top, vec2 size, vec2 minMaxDepth = {0, 1}) {
         D3D11_VIEWPORT viewport = {
            top.x, top.y, size.x, size.y, minMaxDepth.x, minMaxDepth.y
         };
         pContext->RSSetViewports(1, &viewport);
      }

      void BeginEvent(std::string_view name) {
         pContext->BeginEventInt(std::wstring(name.begin(), name.end()).data(), 0);
      }

      void EndEvent() {
         pContext->EndEvent();
      }

      void SetCommonSamplers();

      ID3D11CommandList* GetD3DCommandList() {
         return nullptr;
      }


      ID3D11DeviceContext3* pContext{};

   private:
      std::vector<Ref<Buffer>> dynConstBuffers;
      uint dynConstBufferOffset = 0;

      std::vector<Ref<Buffer>> dynVertBuffers;
      uint dynVertBufferOffset = 0;
   };

   struct GpuMarker {
      GpuMarker(CommandList& cmd, std::string_view name) : cmd(cmd) {
         cmd.BeginEvent(name);
      }

      ~GpuMarker() {
         cmd.EndEvent();
      }

      CommandList& cmd;
   };

#define GPU_MARKER(Name) GpuMarker marker{ cmd, Name };

}
