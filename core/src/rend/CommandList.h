#pragma once
// todo:
#include "Buffer.h"
#include "Device.h"
#include "Texture2D.h"
#include "core/Core.h"

#include <shared/IndirectArgs.hlsli>

namespace pbe {

   struct OffsetedBuffer {
      Buffer* buffer{};
      uint offset = 0;
   };

   class CORE_API CommandList {
   public:
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

         return AllocDynBuffer(data, size, dynConstBuffers);
      }

      OffsetedBuffer AllocDynVertBuffer(const void* data, uint size) {
         return AllocDynBuffer(data, size, dynVertBufffers);
      }

      // todo: remove data
      OffsetedBuffer AllocDynDrawIndexedInstancedBuffer(const void* data, uint count) {
         uint size = count * sizeof(DrawIndexedInstancedArgs);
         return AllocDynBuffer(data, size, dynDrawIndexedInstancedBuffer);
      }

      template<typename T>
      void UpdateSubresource(Buffer& buffer, const T& data, uint offset = 0) {
         UpdateSubresource(buffer, (const void*)&data, offset, sizeof(T));
      }

      void UpdateSubresource(Buffer& buffer, const void* data, uint offset = 0, size_t size = -1);

      void CopyResource(GPUResource& dst, GPUResource& src) {
         pContext->CopyResource(dst.pResource.Get(), src.pResource.Get());
      }

      // void CopySubresourceRegion(GPUResource& dst, uint dstSub, uint dstX, uint dstY, uint dstZ,
      //    GPUResource& src, uint srcSub, const D3D11_BOX& srcBox) {
      //    pContext->CopySubresourceRegion(dst.pResource.Get(), dstSub, dstX, dstY, dstZ,
      //       src.pResource.Get(), srcSub, &srcBox);
      // }

      void CopyBufferRegion(Buffer& dst, uint dstOffset, Buffer& src, uint srcOffset, uint size) {
         D3D11_BOX box{};
         box.left = srcOffset;
         box.right = srcOffset + size;
         box.bottom = 1;
         box.back = 1;

         pContext->CopySubresourceRegion(dst.pResource.Get(), 0, dstOffset, 0, 0, src.pResource.Get(), 0, &box);
      }

      void ReadbackBuffer(Buffer& buffer, uint offset, uint size, void* data) {
         auto dynBuffer = AllocDynBuffer(nullptr, size, dynReadbackBufffers);

         CopyBufferRegion(*dynBuffer.buffer, dynBuffer.offset, buffer, offset, size);

         auto resource = dynBuffer.buffer->pResource.Get();

         // todo: map all subresource, but required only part
         D3D11_MAPPED_SUBRESOURCE mapped;
         pContext->Map(resource, 0, D3D11_MAP_READ, 0, &mapped);

         char* readbackData = ((char*)mapped.pData + dynBuffer.offset);
         memcpy(data, readbackData, size);

         pContext->Unmap(resource, 0);
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

      template<typename T>
      OffsetedBuffer AllocAndSetCommonCB(int slot, const T& data) {
         constexpr uint dataSize = sizeof(T);
         auto dynCB = AllocDynConstantBuffer((const void*)&data, dataSize);
         SetCommonCB(slot, dynCB.buffer, dynCB.offset, dataSize);
         return dynCB;
      }

      void SetCommonCB(int slot, Buffer* buffer, uint offsetInBytes, uint size);

      void SetCommonSamplers();

      ID3D11CommandList* GetD3DCommandList() {
         return nullptr;
      }

      ID3D11DeviceContext3* pContext{};

   private:
      struct DynBuffers {
         Buffer::Desc desc;
         std::vector<Ref<Buffer>> dynBuffers;
         uint dynBufferOffset = 0;
      };

      DynBuffers dynConstBuffers{ .desc = Buffer::Desc::Constant("cmdList dynConstBuffer", 256 * 512)};
      DynBuffers dynVertBufffers{ .desc = Buffer::Desc::Vertex("cmdList dynVertBuffer", 1024)};
      DynBuffers dynReadbackBufffers{ .desc = Buffer::Desc::Readback("cmdList dynReadbackBuffer", 512)};
      DynBuffers dynDrawIndexedInstancedBuffer{
         .desc = Buffer::Desc::IndirectArgs("cmdList dynDrawIndexedInstancedBuffer",
         16 * sizeof(DrawIndexedInstancedArgs))};

      OffsetedBuffer AllocDynBuffer(const void* data, uint size, DynBuffers& dynBuffer) {
         uint descSize = dynBuffer.desc.Size;

         if (dynBuffer.dynBuffers.empty() || dynBuffer.dynBufferOffset + size >= descSize) {
            auto bufferDesc = dynBuffer.desc;
            bufferDesc.name = std::format("{} {}", dynBuffer.desc.name.data(), dynBuffer.dynBuffers.size());
            bufferDesc.Size = std::max(descSize, size);

            dynBuffer.dynBuffers.emplace_back(Buffer::Create(bufferDesc));

            dynBuffer.dynBufferOffset = 0;
         }

         uint oldOffset = dynBuffer.dynBufferOffset;
         dynBuffer.dynBufferOffset += size;

         auto& curDynConstBuffer = *dynBuffer.dynBuffers.back();

         UpdateSubresource(curDynConstBuffer, data, oldOffset, size);

         return { &curDynConstBuffer, oldOffset };
      }

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
