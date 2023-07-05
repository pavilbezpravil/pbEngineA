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

#include "core/CVar.h"
#include "core/Profiler.h"
#include "gui/Gui.h"
#include "fs/FileWatch.h"

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

   CVarValue<bool> cShaderReloadOnAnyChange{ "shaders/reload on any change", true};
   CVarValue<bool> cShaderUseCache{ "shaders/use cache", false};

   size_t StrHash(std::string_view str) {
      return std::hash<std::string_view>()(str);
   }

   std::wstring ToWstr(std::string_view str) {
      return std::wstring(str.begin(), str.end());
   }

   static string gEngineSourcePath = "../../";
   static string gShadersSourcePath = "../../core/shaders";
   static string gShadersCacheFolder = "shader_cache";

   string GetShadersPath(string_view path) {
      return gShadersSourcePath + '/' + path.data();
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

   std::unordered_map<size_t, Shader*> sShadersMap;

   void ReloadShaders() {
      INFO("Reload shaders!");
      for (auto [_, shader] : sShadersMap) {
         shader->Compile(true);
      }
   }

   Shader::Shader(const ShaderDesc& desc) : desc(desc) {}

   static Ref<Shader> ShaderCompile(const ShaderDesc& desc) {
      if (desc.path.empty()) {
         return {};
      }

      std::hash<ShaderDesc> h;
      auto shaderDescHash = h(desc);

      auto it = sShadersMap.find(shaderDescHash);
      if (it != sShadersMap.end()) {
         return it->second;
      }

      Ref shader{ new Shader(desc) };
      shader->Compile();

      sShadersMap[shaderDescHash] = shader;

      return shader;
   }

   bool Shader::Compile(bool force) {
      compiled = false;

      std::hash<ShaderDesc> h;
      auto shaderDescHash = h(desc);

      auto shaderCachePath = std::format("{}/{}", gShadersCacheFolder, shaderDescHash);
      auto wShaderCachePath = ToWstr(shaderCachePath);

      ID3D10Blob* shaderBuffer{};

      if (cShaderUseCache && !force) {
         D3DReadFileToBlob(wShaderCachePath.c_str(), &shaderBuffer);
      }

      if (!shaderBuffer) {
         CompileShader(desc, &shaderBuffer);
         if (!shaderBuffer) {
            return false;
         }

         if (cShaderUseCache) {
            fs::create_directory(gShadersCacheFolder);
            if (D3DWriteBlobToFile(shaderBuffer, wShaderCachePath.c_str(), true) != S_OK) {
               WARN("Cant save shader in cache");
            }
         }
      }

      blob = shaderBuffer;
      shaderBuffer->Release();

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

   void GpuProgram::SetUAV_Dx11(CommandList& cmd, std::string_view name, ID3D11UnorderedAccessView* uav) {
      size_t id = StrHash(name);

      if (cs) {
         const auto reflection = cs->reflection;

         auto iter = reflection.find(id);
         if (iter != reflection.end()) {
            const auto& bi = iter->second;
            if (uav) {
               cmd.pContext->CSSetUnorderedAccessViews(bi.BindPoint, 1, &uav, nullptr);
            } else {
               ID3D11UnorderedAccessView* viewsUAV[] = { nullptr };
               cmd.pContext->CSSetUnorderedAccessViews(bi.BindPoint, _countof(viewsUAV), viewsUAV, nullptr);
            }
         }
      }
   }

   void GpuProgram::SetUAV(CommandList& cmd, std::string_view name, GPUResource* resource) {
      SetUAV_Dx11(cmd, name, resource ? resource->uav.Get() : nullptr);
   }

   void GpuProgram::SetUAV(CommandList& cmd, std::string_view name, GPUResource& resource) {
      SetUAV(cmd, name, &resource);
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

   struct ShaderSrcDesc {
      string path;
      string source;
      int64 hash;

      std::unordered_set<string> includes;
   };

   struct ShaderMng {
      std::unordered_map<string, ShaderSrcDesc> shaderSrcMap;

      void AddSrc(string_view path)
      {
         auto iter = shaderSrcMap.find(path.data());
         if (iter == shaderSrcMap.end()) {
            ShaderSrcDesc desc;
            desc.path = path;

            INFO("ShaderSrcPath = {}", desc.path);

            auto absPath = GetShadersPath(desc.path);

            desc.source = ReadFileAsString(absPath);
            const auto& src = desc.source;

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

                        desc.includes.insert(includePath.data()); // todo:

                        continue;
                     }
                  }

                  WARN("Shader src code error");
               }
            }

            // desc.hash = ;

            shaderSrcMap[path.data()] = desc;
         }
      }
      
   };

   static ShaderMng sShaderMng;

   void OpenVSCodeEngineSource() {
      std::string cmd = std::format("code {}", gEngineSourcePath);
      system(cmd.c_str());
   }

   void ShadersWindow() {
      if (ImGui::Button("Reload")) {
         ReloadShaders();
      }

      if (ImGui::Button("Clear cache")) {
         fs::remove_all(gShadersCacheFolder);
      }

      if (ImGui::Button("Open cache folder")) {
         OpenFileExplorer(gShadersCacheFolder);
      }

      // todo:
      if (ImGui::Button("Open vs code engine folder")) {
         OpenVSCodeEngineSource();
      }

      // todo:
      if (ImGui::Button("Open explorer engine folder")) {
         OpenFileExplorer(".\\..\\..\\");
      }

      // CALL_ONCE([] {
      //       int64 mask = 1 << 7 | 1 << 13 | 1 << 25;
      //
      //       INFO("Mask {}", mask);
      //
      //       unsigned long index;
      //       while (BitScanForward64(&index, mask)) {
      //          mask &= ~(1 << index);
      //          INFO("{}", index);
      //       }
      // });

      ImGui::Text("Shaders:");

      for (auto [_, shader] : sShadersMap) {
         const auto& desc = shader->desc;
         std::string shaderName = desc.path + " " + desc.entryPoint;

         UI_PUSH_ID(&desc);

         if (UI_TREE_NODE(shaderName.c_str())) {
            ImGui::Text("%s type %d", desc.entryPoint.c_str(), desc.type);

            if (ImGui::Button("Edit")) {
               OpenVSCodeEngineSource();

               // open shader file
               auto cmd = std::format("code {}/{}", gShadersSourcePath, desc.path);
               system(cmd.c_str());
            }

            if (UI_TREE_NODE("Defines")) {
               const auto& defines = desc.defines;

               int nDefines = defines.NDefines();
               for (int i = 0; i < nDefines; ++i) {
                  ImGui::Text("%s = %s", defines[2 * i].Name, defines[2 * i + 1].Definition);
               }
            }

            if (UI_TREE_NODE("Reflection")) {
               for (auto [id, bindDesc] : shader->reflection) {
                  ImGui::Text("%s %d, type %d", bindDesc.Name, bindDesc.BindPoint, bindDesc.Type);
               }
            }
         }
      }
   }

   void ShadersSrcWatcherUpdate()
   {
      static bool anySrcChanged = false; // todo: atomic

      CALL_ONCE([] {
         // todo:
         static filewatch::FileWatch<std::string> watch(
            gShadersSourcePath,
            [](const std::string& path, const filewatch::Event change_type) {
               anySrcChanged = true;
               std::cout << path << " : ";
               switch (change_type) {
               case filewatch::Event::added:
                  std::cout << "The file was added to the directory." << '\n';
                  break;
               case filewatch::Event::removed:
                  std::cout << "The file was removed from the directory." << '\n';
                  break;
               case filewatch::Event::modified:
                  std::cout << "The file was modified. This can be a change in the time stamp or attributes." << '\n';
                  break;
               case filewatch::Event::renamed_old:
                  std::cout << "The file was renamed and this is the old name." << '\n';
                  break;
               case filewatch::Event::renamed_new:
                  std::cout << "The file was renamed and this is the new name." << '\n';
                  break;
               }
            }
         );
      });

      if (anySrcChanged && cShaderReloadOnAnyChange) {
         ReloadShaders();
         anySrcChanged = false;
      }
   }
}
