#include "Shader.h"

#include <d3d11shader.h>
#include <d3dcompiler.h>
#include "Device.h"
#include "core/Log.h"
#include "Texture2D.h"
#include "CommandList.h"
#include "core/Assert.h"

static const char* gShaderProfile[] = {
   "vs_5_1",
   "ps_5_1",
   "cs_5_1",
};

size_t StrHash(std::string_view str) {
   return std::hash<std::string_view>()(str);
}

std::wstring ToWstr(std::string_view str) {
   return std::wstring(str.begin(), str.end());
}

HRESULT CompileShader(std::string_view srcFile, LPCSTR entryPoint, LPCSTR profile, ID3DBlob** blob) {
   if (!entryPoint || !profile || !blob)
      return E_INVALIDARG;

   *blob = nullptr;

   UINT flags = D3DCOMPILE_ENABLE_STRICTNESS & D3DCOMPILE_ALL_RESOURCES_BOUND;
#if defined(DEBUG)
   flags |= D3DCOMPILE_DEBUG;
#endif

   const D3D_SHADER_MACRO defines[] = {"EXAMPLE_DEFINE", "1", NULL, NULL};

   auto wsrcPath = ToWstr(srcFile);

   INFO("Compile shader '{}' entryPoint: '{}' profile: '{}'", srcFile, entryPoint, profile);

   ID3DBlob* shaderBlob = nullptr;
   ID3DBlob* errorBlob = nullptr;
   HRESULT hr = D3DCompileFromFile(wsrcPath.data(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, profile, flags, 0,
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

void ShaderCompileTest() {
   auto desc = ProgramDesc::Cs("test.cs", "main");
   auto program = GpuProgram(desc);
}

Ref<Shader> ShaderCompile(ShaderDesc& desc) {
   if (desc.path.empty()) {
      return {};
   }

   ID3D10Blob* shaderBuffer;
   CompileShader(desc.path, desc.entryPoint.data(), gShaderProfile[(int)desc.type], &shaderBuffer);
   if (!shaderBuffer) {
      return {};
   }

   Ref shader{ new Shader() };

   // https://gist.github.com/d7samurai/261c69490cce0620d0bfc93003cd1052
   // ID3D11Device1* device;
   // baseDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&device));
   //
   // ID3D11DeviceContext1* deviceContext;
   // baseDeviceContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&deviceContext));

   switch (desc.type) {
      case ShaderType::Vertex:
         sDevice->g_pd3dDevice->CreateVertexShader(shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(),
            nullptr, &shader->vs);
      break;
      case ShaderType::Pixel:
         sDevice->g_pd3dDevice->CreatePixelShader(shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(),
            nullptr, &shader->ps);
      break;
      case ShaderType::Compute:
         sDevice->g_pd3dDevice->CreateComputeShader(shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(),
                                    nullptr, &shader->cs);
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
      shader->reflection[id] = bindDesc;
   }

   return shader;
}

GpuProgram::GpuProgram(ProgramDesc& desc) : desc(desc) {
   vs = ShaderCompile(desc.vs);
   ps = ShaderCompile(desc.ps);
   cs = ShaderCompile(desc.cs);
}

void GpuProgram::SetTexture(CommandList& cmd, std::string_view name, Texture2D& texture) {
   size_t id = StrHash(name);

   if (vs) {
      const auto& bi = vs->reflection[id];
      // cmd.pContext->VSSetShaderResources(bi.BindPoint, 1, &texture.srv);
   }
}

void GpuProgram::Dispatch(CommandList& cmd, int3 groups) {
   cmd.pContext->Dispatch(groups.x, groups.y, groups.z);
}
