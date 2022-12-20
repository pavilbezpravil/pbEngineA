#include "pch.h"
#include "Shader.h"
#include "Device.h"
#include "core/Log.h"
#include "Texture2D.h"
#include "Buffer.h"
#include "CommandList.h"
#include "core/Assert.h"
#include "fs/FileSystem.h"

#include <d3d11shader.h>
#include <d3dcompiler.h>

#include "core/Profiler.h"

// https://stackoverflow.com/questions/19195183/how-to-properly-hash-the-custom-struct
template <class T>
void HashCombine(std::size_t& s, const T& v) {
   std::hash<T> h;
   s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
}

template<>
struct std::hash<D3D_SHADER_MACRO> {
   std::size_t operator()(const D3D_SHADER_MACRO& v) const {
      std::size_t res = 0;
      HashCombine(res, v.Name);
      HashCombine(res, v.Definition);
      return res;
   }
};

// todo: any integer
template<>
struct std::hash<std::vector<int>> {
   std::size_t operator()(const std::vector<int>& v) const {
      std::size_t res = v.size();
      for (auto& i : v) {
         res ^= i + 0x9e3779b9 + (res << 6) + (res >> 2);
      }
      return res;
   }
};

template<typename T>
struct std::hash<std::vector<T>> {
   std::size_t operator()(const std::vector<T>& v) const {
      std::size_t res = v.size();
      for (auto& i : v) {
         HashCombine(res, i);
      }
      return res;
   }
};

template<>
struct std::hash<pbe::ShaderDesc> {
   std::size_t operator()(const pbe::ShaderDesc& v) const {
      std::size_t res = 0;
      HashCombine(res, v.entryPoint);
      HashCombine(res, v.path);
      HashCombine(res, v.type);
      HashCombine(res, *(std::vector<D3D_SHADER_MACRO>*) & v.defines);
      return res;
   }
};

template<>
struct std::hash<pbe::ProgramDesc> {
   std::size_t operator()(const pbe::ProgramDesc& v) const {
      std::size_t res = 0;
      HashCombine(res, v.vs);
      HashCombine(res, v.hs);
      HashCombine(res, v.ds);
      HashCombine(res, v.ps);

      HashCombine(res, v.cs);
      return res;
   }
};

namespace pbe {

   size_t StrHash(std::string_view str) {
      return std::hash<std::string_view>()(str);
   }

   std::wstring ToWstr(std::string_view str) {
      return std::wstring(str.begin(), str.end());
   }

   string GetShadersPath(string_view path) {
      static string gAssetsPath = "../../core/shaders/";
      return gAssetsPath + path.data();
   }

   HRESULT CompileShader(ShaderDesc& desc, ID3DBlob** blob) {
      static const char* gShaderProfile[] = {
         "vs_5_0",
         "hs_5_0",
         "ds_5_0",
         "ps_5_0",
         "cs_5_0",
      };

      auto profile = gShaderProfile[(int)desc.type];

      auto path = GetShadersPath(desc.path);

      if (!fs::exists(path)) {
         WARN("Cant find file '{}' for compilation", desc.path);
      }

      // todo: 
      if (0) {
         INFO("ShaderSrcPath = {}", desc.path);

         auto src = ReadFileAsString(path);

         size_t pos = 0;
         while (pos != std::string::npos) {
            pos = src.find("#include """, pos);

            if (pos != std::string::npos) {
               pos = src.find('"', pos + 1);
               if (pos != std::string::npos) {
                  auto startIdx = pos + 1;
                  pos = src.find('"', pos + 1);

                  if (pos != std::string::npos) {
                     string_view includePath{ src.data() + startIdx, pos - startIdx };
                     INFO("   IncludePath = {}", includePath);

                     continue;
                  }
               }

               WARN("Shader src code error");
            }
         }
      }

      *blob = nullptr;

      UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
      // D3DCOMPILE_ALL_RESOURCES_BOUND
#if defined(DEBUG)
      flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

      const D3D_SHADER_MACRO* defines = desc.defines.data();

      auto wsrcPath = ToWstr(path);

      CpuTimer timer;

      ID3DBlob* shaderBlob = nullptr;
      ID3DBlob* errorBlob = nullptr;
      HRESULT hr = D3DCompileFromFile(wsrcPath.data(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, desc.entryPoint.data(), profile,
                                      flags, 0,
                                      &shaderBlob, &errorBlob);

      if (FAILED(hr)) {
         if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            INFO("Erorr:");
            WARN((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
         }

         if (shaderBlob)
            shaderBlob->Release();

         return hr;
      }

      INFO("Compiled shader '{}' entryPoint: '{}'. Compile time {} ms.", desc.path, desc.entryPoint, timer.ElapsedMs());

      *blob = shaderBlob;

      return hr;
   }

   std::vector<Shader*> sShaders;

   void ReloadShaders() {
      INFO("Reload shaders!");
      for (auto shader : sShaders) {
         shader->Compile();
      }
   }

   Shader::Shader(const ShaderDesc& desc) : desc(desc) {}

   static Ref<Shader> ShaderCompile(const ShaderDesc& desc) {
      if (desc.path.empty()) {
         return {};
      }

      // todo: check if already exist
      Ref shader{ new Shader(desc) };
      shader->Compile();
      sShaders.push_back(shader);
      return shader;
   }

   bool Shader::Compile() {
      compiled = false;

      ID3D10Blob* shaderBuffer{};
      CompileShader(desc, &shaderBuffer);
      if (!shaderBuffer) {
         return false;
      }

      blob = shaderBuffer;

      std::string_view dbgName = desc.path.data();

      switch (desc.type) {
      case ShaderType::Vertex: sDevice->g_pd3dDevice->CreateVertexShader(
         shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(),
         nullptr, &vs);
         SetDbgName(vs.Get(), dbgName);
         break;
      case ShaderType::Hull: sDevice->g_pd3dDevice->CreateHullShader(
         shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(),
         nullptr, &hs);
         SetDbgName(hs.Get(), dbgName);
         break;
      case ShaderType::Domain: sDevice->g_pd3dDevice->CreateDomainShader(
         shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(),
         nullptr, &ds);
         SetDbgName(ds.Get(), dbgName);
         break;
      case ShaderType::Pixel: sDevice->g_pd3dDevice->CreatePixelShader(
         shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(),
         nullptr, &ps);
         SetDbgName(ps.Get(), dbgName);
         break;
      case ShaderType::Compute: sDevice->g_pd3dDevice->CreateComputeShader(
         shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(),
         nullptr, &cs);
         SetDbgName(cs.Get(), dbgName);
         break;
      default:
         UNIMPLEMENTED();
      }

      // INFO("Reflection:");

      ID3D11ShaderReflection* pReflector = NULL;
      D3DReflect(shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(), IID_ID3D11ShaderReflection,
         (void**)&pReflector);

      int resourceIdx = 0;
      D3D11_SHADER_INPUT_BIND_DESC bindDesc;
      while (SUCCEEDED(pReflector->GetResourceBindingDesc(resourceIdx++, &bindDesc))) {
         // INFO("\tName: {} Type: {} BindPoint: {}", bindDesc.Name, bindDesc.Type, bindDesc.BindPoint);

         size_t id = StrHash(bindDesc.Name);
         reflection[id] = bindDesc;
      }

      compiled = true;
      return compiled;
   }

   Ref<GpuProgram> GpuProgram::Create(const ProgramDesc& desc) {
      return Ref<GpuProgram>::Create(desc);
   }

   void GpuProgram::Activate(CommandList& cmd) {
      bool graphics = vs || ps;
      if (graphics) {
         cmd.pContext->VSSetShader(vs ? vs->vs.Get() : nullptr, nullptr, 0);
         cmd.pContext->HSSetShader(hs ? hs->hs.Get() : nullptr, nullptr, 0);
         cmd.pContext->DSSetShader(ds ? ds->ds.Get() : nullptr, nullptr, 0);
         cmd.pContext->PSSetShader(ps ? ps->ps.Get() : nullptr, nullptr, 0);
      } else {
         if (cs) {
            cmd.pContext->CSSetShader(cs->cs.Get(), nullptr, 0);
         }
      }
   }

   void GpuProgram::SetCB(CommandList& cmd, std::string_view name, Buffer& buffer, uint offsetInBytes, uint size) {
      size_t id = StrHash(name);

      ID3D11Buffer* dxBuffer = buffer.GetBuffer();

      offsetInBytes /= 16;

      if (vs) {
         const auto reflection = vs->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            cmd.pContext->VSSetConstantBuffers1(bi.BindPoint, 1, &dxBuffer, &offsetInBytes, &size);
         }
      }

      if (hs) {
         const auto reflection = hs->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            cmd.pContext->HSSetConstantBuffers1(bi.BindPoint, 1, &dxBuffer, &offsetInBytes, &size);
         }
      }

      if (ds) {
         const auto reflection = ds->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            cmd.pContext->DSSetConstantBuffers1(bi.BindPoint, 1, &dxBuffer, &offsetInBytes, &size);
         }
      }

      if (ps) {
         const auto reflection = ps->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            cmd.pContext->PSSetConstantBuffers1(bi.BindPoint, 1, &dxBuffer, &offsetInBytes, &size);
         }
      }

      if (cs) {
         const auto reflection = cs->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            cmd.pContext->CSSetConstantBuffers1(bi.BindPoint, 1, &dxBuffer, &offsetInBytes, &size);
         }
      }
   }

   void GpuProgram::SetSRV(CommandList& cmd, std::string_view name, ID3D11ShaderResourceView* srv) {
      size_t id = StrHash(name);

      if (vs) {
         const auto reflection = vs->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            cmd.pContext->VSSetShaderResources(bi.BindPoint, 1, &srv);
         }
      }
      if (hs) {
         const auto reflection = hs->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            cmd.pContext->HSSetShaderResources(bi.BindPoint, 1, &srv);
         }
      }
      if (ds) {
         const auto reflection = ds->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            cmd.pContext->DSSetShaderResources(bi.BindPoint, 1, &srv);
         }
      }
      if (ps) {
         const auto reflection = ps->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            cmd.pContext->PSSetShaderResources(bi.BindPoint, 1, &srv);
         }
      }

      if (cs) {
         const auto reflection = cs->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            cmd.pContext->CSSetShaderResources(bi.BindPoint, 1, &srv);
         }
      }
   }

   void GpuProgram::SetSRV(CommandList& cmd, std::string_view name, GPUResource& resource) {
      SetSRV(cmd, name, resource.srv.Get());
   }

   void GpuProgram::SetUAV(CommandList& cmd, std::string_view name, ID3D11UnorderedAccessView* uav) {
      size_t id = StrHash(name);

      if (cs) {
         const auto reflection = cs->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            cmd.pContext->CSSetUnorderedAccessViews(bi.BindPoint, 1, &uav, nullptr);
         }
      }
   }

   void GpuProgram::SetUAV(CommandList& cmd, std::string_view name, GPUResource& resource) {
      SetUAV(cmd, name, resource.uav.Get());
   }

   void GpuProgram::DrawInstanced(CommandList& cmd, uint vertCount, uint instCount, uint startVert) {
      cmd.pContext->DrawInstanced(vertCount, instCount, startVert, 0);
   }

   void GpuProgram::DrawIndexedInstanced(CommandList& cmd, uint indexCount, uint instCount, uint indexStart, uint startVert) {
      cmd.pContext->DrawIndexedInstanced(indexCount, instCount, indexStart, startVert, 0);
   }

   void GpuProgram::DrawIndexedInstancedIndirect(CommandList& cmd, Buffer& args, uint offset) {
      cmd.pContext->DrawIndexedInstancedIndirect(args.GetBuffer(), offset);
   }

   void GpuProgram::Dispatch3D(CommandList& cmd, int3 groups) {
      cmd.pContext->Dispatch(groups.x, groups.y, groups.z);
   }

   bool GpuProgram::Valid() const {
      return (!vs || vs->Valid()) && (!hs || hs->Valid()) && (!ds || ds->Valid()) && (!ps || ps->Valid()) && (!cs || cs->Valid());
   }

   GpuProgram::GpuProgram(const ProgramDesc& desc) : desc(desc) {
      vs = ShaderCompile(desc.vs);
      hs = ShaderCompile(desc.hs);
      ds = ShaderCompile(desc.ds);
      ps = ShaderCompile(desc.ps);
      cs = ShaderCompile(desc.cs);
   }

   static std::unordered_map<ProgramDesc, Ref<GpuProgram>> sGpuPrograms;

   GpuProgram* GetGpuProgram(const ProgramDesc& desc) {
      auto it = sGpuPrograms.find(desc);
      if (it == sGpuPrograms.end()) {
         auto program = GpuProgram::Create(desc);
         sGpuPrograms[desc] = program;
         return program.Raw();
      }

      return it->second.Raw();
   }

   void TermGpuPrograms() {
      sGpuPrograms.clear();
   }

}
