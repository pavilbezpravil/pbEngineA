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

   HRESULT CompileShader(std::string_view srcFile, LPCSTR entryPoint, LPCSTR profile, ID3DBlob** blob) {
      INFO("Compile shader '{}' entryPoint: '{}' profile: '{}'", srcFile, entryPoint, profile);

      if (!entryPoint || !profile || !blob)
         return E_INVALIDARG;


      auto path = GetShadersPath(srcFile);

      if (!fs::exists(path)) {
         WARN("Cant find file '{}'", srcFile);
      }

      *blob = nullptr;

      UINT flags = D3DCOMPILE_ENABLE_STRICTNESS & D3DCOMPILE_ALL_RESOURCES_BOUND;
#if defined(DEBUG)
      flags |= D3DCOMPILE_DEBUG;
#endif

      const D3D_SHADER_MACRO defines[] = {"EXAMPLE_DEFINE", "1", NULL, NULL};

      auto wsrcPath = ToWstr(path);

      ID3DBlob* shaderBlob = nullptr;
      ID3DBlob* errorBlob = nullptr;
      HRESULT hr = D3DCompileFromFile(wsrcPath.data(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, profile,
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

      INFO("Successful!");

      *blob = shaderBlob;

      return hr;
   }

   static std::vector<Shader*> shaders;

   void ReloadShaders() {
      INFO("Reload shaders!");
      for (auto shader : shaders) {
         shader->Compile();
      }
   }

   Shader::Shader(ShaderDesc& desc) : desc(desc) {}

   Ref<Shader> ShaderCompile(ShaderDesc& desc) {
      if (desc.path.empty()) {
         return {};
      }

      // todo: check if already exist
      Ref shader{ new Shader(desc) };
      shader->Compile();
      shaders.push_back(shader);
      return shader;
   }

   bool Shader::Compile() {
      compiled = false;

      static const char* gShaderProfile[] = {
         "vs_5_0",
         "ps_5_0",
         "cs_5_0",
      };

      ID3D10Blob* shaderBuffer{};
      CompileShader(desc.path, desc.entryPoint.data(), gShaderProfile[(int)desc.type], &shaderBuffer);
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

      INFO("Reflection:");

      ID3D11ShaderReflection* pReflector = NULL;
      D3DReflect(shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(), IID_ID3D11ShaderReflection,
         (void**)&pReflector);

      int resourceIdx = 0;
      D3D11_SHADER_INPUT_BIND_DESC bindDesc;
      while (SUCCEEDED(pReflector->GetResourceBindingDesc(resourceIdx++, &bindDesc))) {
         INFO("\tName: {} Type: {} BindPoint: {}", bindDesc.Name, bindDesc.Type, bindDesc.BindPoint);

         size_t id = StrHash(bindDesc.Name);
         reflection[id] = bindDesc;
      }

      compiled = true;
      return compiled;
   }

   Ref<GpuProgram> GpuProgram::Create(ProgramDesc& desc) {
      return Ref<GpuProgram>::Create(desc);
   }

   void GpuProgram::Activate(CommandList& cmd) {
      if (vs) {
         cmd.pContext->VSSetShader(vs->vs.Get(), nullptr, 0);
      }

      if (ps) {
         cmd.pContext->PSSetShader(ps->ps.Get(), nullptr, 0);
      }

      if (cs) {
         cmd.pContext->CSSetShader(cs->cs.Get(), nullptr, 0);
      }
   }

   void GpuProgram::SetConstantBuffer(CommandList& cmd, std::string_view name, Buffer& buffer) {
      size_t id = StrHash(name);

      ID3D11Buffer* dxBuffer = buffer.GetBuffer();

      if (vs) {
         const auto reflection = vs->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            cmd.pContext->VSSetConstantBuffers(bi.BindPoint, 1, &dxBuffer);
         }
      }

      if (ps) {
         const auto reflection = ps->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            cmd.pContext->PSSetConstantBuffers(bi.BindPoint, 1, &dxBuffer);
         }
      }

      if (cs) {
         const auto reflection = cs->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            cmd.pContext->CSSetConstantBuffers(bi.BindPoint, 1, &dxBuffer);
         }
      }
   }

   void GpuProgram::SetSrvBuffer(CommandList& cmd, std::string_view name, Buffer& buffer) {
      size_t id = StrHash(name);

      if (vs) {
         const auto reflection = vs->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            cmd.pContext->VSSetShaderResources(bi.BindPoint, 1, buffer.srv.GetAddressOf());
         }
      }

      if (ps) {
         const auto reflection = ps->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            cmd.pContext->PSSetShaderResources(bi.BindPoint, 1, buffer.srv.GetAddressOf());
         }
      }
   }

   void GpuProgram::SetTexture(CommandList& cmd, std::string_view name, Texture2D& texture) {
      size_t id = StrHash(name);

      if (vs) {
         const auto& bi = vs->reflection[id];
         // cmd.pContext->VSSetShaderResources(bi.BindPoint, 1, &texture.srv);
      }
   }

   void GpuProgram::DrawInstanced(CommandList& cmd, int vertCount, int instCount, int startVert) {
      cmd.pContext->DrawInstanced(vertCount, instCount, startVert, 0);
   }

   void GpuProgram::DrawIndexedInstanced(CommandList& cmd, int indexCount, int instCount, int indexStart, int startVert) {
      cmd.pContext->DrawIndexedInstanced(indexCount, instCount, indexStart, startVert, 0);
   }

   void GpuProgram::Dispatch(CommandList& cmd, int3 groups) {
      cmd.pContext->Dispatch(groups.x, groups.y, groups.z);
   }

   bool GpuProgram::Valid() const {
      return (!vs || vs->Valid()) && (!ps || ps->Valid()) && (!cs || cs->Valid());
   }

   GpuProgram::GpuProgram(ProgramDesc& desc) : desc(desc) {
      vs = ShaderCompile(desc.vs);
      ps = ShaderCompile(desc.ps);
      cs = ShaderCompile(desc.cs);
   }

}