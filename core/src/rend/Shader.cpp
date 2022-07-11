#include "Shader.h"

#include <d3d11shader.h>
#include <d3dcompiler.h>
#include "Device.h"
#include "core/Log.h"


HRESULT CompileShader(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ ID3DBlob** blob) {
   if (!srcFile || !entryPoint || !profile || !blob)
      return E_INVALIDARG;

   *blob = nullptr;

   UINT flags = D3DCOMPILE_ENABLE_STRICTNESS & D3DCOMPILE_ALL_RESOURCES_BOUND;
#if defined(DEBUG)
   flags |= D3DCOMPILE_DEBUG;
#endif

   const D3D_SHADER_MACRO defines[] = {"EXAMPLE_DEFINE", "1", NULL, NULL};

   ID3DBlob* shaderBlob = nullptr;
   ID3DBlob* errorBlob = nullptr;
   HRESULT hr = D3DCompileFromFile(srcFile, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, profile, flags, 0,
                                   &shaderBlob, &errorBlob);

   if (FAILED(hr)) {
      if (errorBlob) {
         OutputDebugStringA((char*)errorBlob->GetBufferPointer());
         WARN((char*)errorBlob->GetBufferPointer());
         errorBlob->Release();
      }

      if (shaderBlob)
         shaderBlob->Release();

      return hr;
   }

   *blob = shaderBlob;

   return hr;
}


void Compile() {
   ID3D10Blob* shaderBuffer;
   CompileShader(L"test.cs", "main", "cs_5_1", &shaderBuffer);

   if (!shaderBuffer) {
      return;
   }

   INFO("test.cs compiled!");

   // ID3D11ComputeShader* shader;
   // sDevice->g_pd3dDevice->CreateComputeShader(shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(),
   //                               nullptr, &shader);

   ID3D11ShaderReflection* pReflector = NULL;
   D3DReflect(shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(), IID_ID3D11ShaderReflection,
              (void**)&pReflector);

   int resourceIdx = 0;
   D3D11_SHADER_INPUT_BIND_DESC desc;
   while (SUCCEEDED(pReflector->GetResourceBindingDesc(resourceIdx++, &desc))) {
      INFO("Name: {} Type: {} BindPoint: {}", desc.Name, desc.Type, desc.BindPoint);
   }
}

