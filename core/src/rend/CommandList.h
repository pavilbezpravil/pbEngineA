#pragma once
// todo:
#include "Buffer.h"
#include "Device.h"
#include "Texture2D.h"
#include "core/Core.h"

#include <shared/IndirectArgs.hlsli>

#include "BindPoint.h"
#include "core/Assert.h"

namespace pbe {
   class GpuProgram;

   struct OffsetedBuffer {
      Buffer* buffer{};
      uint offset = 0;
   };

   struct CmdBind {
      enum Type {
         SRV,
         UAV,
         CB,
      } type;

      BindPoint bindPoint;
   };

   struct CmdBinds {
      std::vector<CmdBind> binds;
   };

   class CORE_API CommandList {
   public:
      CommandList(ID3D11DeviceContext3* pContext) : pContext(pContext) {

      }

      // CommandList() {
      //    sDevice->g_pd3dDevice->CreateDeferredContext(0, &pContext);
      // }

      bool SetCompute(GpuProgram& compute);

      template<typename T>
      void SetCB(const BindPoint& bind, Buffer* buffer = nullptr, uint offsetInBytes = 0) {
         SetCB(bind, buffer, offsetInBytes, sizeof(T));
      }

      // todo: i suppose for dx12 size unnecessary
      void SetCB(const BindPoint& bind, Buffer* buffer, uint offsetInBytes, uint size) {
         if (!bind) {
            return;
         }

         if (buffer) {
            AddBindToGuard(CmdBind::CB, bind);
         }

         ID3D11Buffer* dxBuffer[] = { buffer ? buffer->GetBuffer() : nullptr };

         offsetInBytes /= 16;

         pContext->VSSetConstantBuffers1(bind.slot, _countof(dxBuffer), dxBuffer, &offsetInBytes, &size);
         pContext->HSSetConstantBuffers1(bind.slot, _countof(dxBuffer), dxBuffer, &offsetInBytes, &size);
         pContext->DSSetConstantBuffers1(bind.slot, _countof(dxBuffer), dxBuffer, &offsetInBytes, &size);
         pContext->PSSetConstantBuffers1(bind.slot, _countof(dxBuffer), dxBuffer, &offsetInBytes, &size);

         pContext->CSSetConstantBuffers1(bind.slot, _countof(dxBuffer), dxBuffer, &offsetInBytes, &size);
      }

      template<typename T>
      OffsetedBuffer AllocAndSetCB(const BindPoint& bind, const T& data) {
         constexpr uint dataSize = sizeof(T);
         auto dynCB = AllocDynConstantBuffer((const void*)&data, dataSize);
         SetCB(bind, dynCB.buffer, dynCB.offset, dataSize);
         return dynCB;
      }

      void SetSRV_Dx11(const BindPoint& bind, ID3D11ShaderResourceView* srv) {
         if (!bind) {
            return;
         }

         if (srv) {
            AddBindToGuard(CmdBind::SRV, bind);
         }

         ID3D11ShaderResourceView* viewsSRV[] = { srv };

         pContext->VSSetShaderResources(bind.slot, _countof(viewsSRV), viewsSRV);
         pContext->HSSetShaderResources(bind.slot, _countof(viewsSRV), viewsSRV);
         pContext->DSSetShaderResources(bind.slot, _countof(viewsSRV), viewsSRV);
         pContext->PSSetShaderResources(bind.slot, _countof(viewsSRV), viewsSRV);

         pContext->CSSetShaderResources(bind.slot, _countof(viewsSRV), viewsSRV);
      }

      void SetSRV(const BindPoint& bind, GPUResource* resource = nullptr) {
         SetSRV_Dx11(bind, resource ? resource->srv.Get() : nullptr);
      }

      void SetUAV_Dx11(const BindPoint& bind, ID3D11UnorderedAccessView* uav) {
         if (!bind) {
            return;
         }

         if (uav) {
            AddBindToGuard(CmdBind::UAV, bind);
         }

         ID3D11UnorderedAccessView* viewsUAV[] = { uav };
         pContext->CSSetUnorderedAccessViews(bind.slot, _countof(viewsUAV), viewsUAV, nullptr);
      }
      void SetUAV(const BindPoint& bind, GPUResource* resource = nullptr) {
         SetUAV_Dx11(bind, resource ? resource->uav.Get() : nullptr);
      }

      void DrawInstanced(uint vertCount, uint instCount, uint startVert) {
         pContext->DrawInstanced(vertCount, instCount, startVert, 0);
      }
      void DrawIndexedInstanced(uint indexCount, uint instCount, uint indexStart, uint startVert) {
         pContext->DrawIndexedInstanced(indexCount, instCount, indexStart, startVert, 0);
      }
      void DrawIndexedInstancedIndirect(Buffer& args, uint offset) {
         pContext->DrawIndexedInstancedIndirect(args.GetBuffer(), offset);
      }

      void Dispatch3D(int3 groups) {
         pContext->Dispatch(groups.x, groups.y, groups.z);
      }
      void Dispatch3D(int3 size, int3 groupSize) {
         auto groups = glm::ceil(vec3{ size } / vec3{ groupSize });
         Dispatch3D(int3{ groups });
      }

      void Dispatch2D(int2 groups) {
         Dispatch3D(int3{ groups, 1 });
      }
      void Dispatch2D(int2 size, int2 groupSize) {
         auto groups = glm::ceil(vec2{ size } / vec2{ groupSize });
         Dispatch2D(groups);
      }

      void Dispatch1D(uint size) {
         Dispatch3D(int3{ size, 1, 1 });
      }

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

      OffsetedBuffer AllocDynDrawIndexedInstancedBuffer(const DrawIndexedInstancedArgs& args, uint count) {
         uint size = count * sizeof(DrawIndexedInstancedArgs);
         return AllocDynBuffer(&args, size, dynDrawIndexedInstancedBuffer);
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

      void ClearUAVFloat(GPUResource& r, const vec4& v = vec4_Zero) {
         pContext->ClearUnorderedAccessViewFloat(r.uav.Get(), &v.x);
      }

      void ClearUAVUint(GPUResource& r, const uint4& v = uint4{0}) {
         pContext->ClearUnorderedAccessViewUint(r.uav.Get(), &v.x);
      }

      void ClearDepthTarget(Texture2D& depth, float depthValue, uint8 stencilValue = 0, uint clearFlags = D3D11_CLEAR_DEPTH) {
         pContext->ClearDepthStencilView(depth.dsv.Get(), clearFlags, depthValue, stencilValue);
      }

      void ClearRenderTarget(Texture2D& rt, const vec4& color) {
         pContext->ClearRenderTargetView(rt.rtv.Get(), &color.x);
      }

      void SetRenderTargets(uint nRT, Texture2D** rt = nullptr, Texture2D* depth = nullptr) {
         ASSERT(nRT <= 4);
         ID3D11RenderTargetView* rtvs[4];
         for (uint i = 0; i < nRT; i++) {
            rtvs[i] = rt[i]->rtv.Get();
         }

         ID3D11DepthStencilView* dsv = depth ? depth->dsv.Get() : nullptr;

         pContext->OMSetRenderTargets(nRT, rtvs, dsv);
      }

      void SetRenderTargets(Texture2D* rt = nullptr, Texture2D* depth = nullptr) {
         int nRtvs = rt ? 1 : 0;
         ID3D11RenderTargetView** rtv = rt ? rt->rtv.GetAddressOf() : nullptr;
         ID3D11DepthStencilView* dsv = depth ? depth->dsv.Get() : nullptr;

         pContext->OMSetRenderTargets(nRtvs, rtv, dsv);
      }

      void SetRenderTargetsUAV(Texture2D* rt = nullptr, Texture2D* depth = nullptr, GPUResource* uav = nullptr) {
         int nRtvs = rt ? 1 : 0;
         ID3D11RenderTargetView** rtv = rt ? rt->rtv.GetAddressOf() : nullptr;
         ID3D11DepthStencilView* dsv = depth ? depth->dsv.Get() : nullptr;

         uint uavInitialCount = 0;
         pContext->OMSetRenderTargetsAndUnorderedAccessViews(nRtvs, rtv,
            dsv, 1, 1, uav ? uav->uav.GetAddressOf() : nullptr, &uavInitialCount);
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

      // todo: first 10
      void ClearSRV_CS();
      void ClearUAV_CS();

      void BeginEvent(std::string_view name) {
         pContext->BeginEventInt(std::wstring(name.begin(), name.end()).data(), 0);
      }

      void EndEvent() {
         pContext->EndEvent();
      }

      void SetCommonSamplers();

      void PushBindsGuard() {
         bindsGuardStack.push_back({});
         currentBindsGuard = &bindsGuardStack.back();
      }

      void PopBindsGuard() {
         ASSERT(currentBindsGuard);
         for (auto& bind : currentBindsGuard->binds) {
            switch (bind.type) {
            case CmdBind::SRV:
               SetSRV(bind.bindPoint);
               break;
            case CmdBind::UAV:
               SetUAV(bind.bindPoint);
               break;
            case CmdBind::CB:
               // todo: mb does not work or leads to crash)
               SetCB(bind.bindPoint, nullptr, 0, 0);
               break;
            }
         }

         bindsGuardStack.pop_back();
         if (bindsGuardStack.empty()) {
            currentBindsGuard = {};
         } else {
            currentBindsGuard = &bindsGuardStack.back();
         }
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

      void AddBindToGuard(CmdBind::Type type, BindPoint bindPoint) {
         if (currentBindsGuard) {
            currentBindsGuard->binds.push_back({ type, bindPoint });
         }
      }

      std::vector<CmdBinds> bindsGuardStack;
      CmdBinds* currentBindsGuard = nullptr;
   };

   struct CmdBindsGuard {
      CmdBindsGuard(CommandList& cmd);
      ~CmdBindsGuard();

      CommandList& cmd;
   };

   // todo: remove
#define CMD_BINDS_GUARD() CmdBindsGuard cmdBindsGuard { cmd };

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
