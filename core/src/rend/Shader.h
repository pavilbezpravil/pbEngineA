#pragma once
#include <d3d11shader.h>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common.h"
#include "core/Core.h"
#include "core/Ref.h"
#include "math/Types.h"

struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11ComputeShader;

namespace pbe {
   class GPUResource;
   class Buffer;

   class Texture2D;
   class CommandList;

   void CORE_API ReloadShaders();

   struct ShaderDefine {
      std::string define;
      std::string value;
   };

   struct ShaderDefines : public std::vector<D3D_SHADER_MACRO> {
      void AddDefine(string_view name, string_view value = {}) {
         if (empty()) {
            emplace_back(name.data(), value.data());
         } else {
            back() = { name.data(), value.data() };
         }
         push_back({});
      }

      friend bool operator==(const ShaderDefines& lhs, const ShaderDefines& rhs) {
         if (lhs.size() != rhs.size()) {
            return false;
         }
         // last define always null
         for (int i = 0; i < (int)lhs.size() - 1; ++i) {
            auto& l = lhs[i];
            auto& r = rhs[i];

            if (strcmp(l.Name, r.Name) != 0 || l.Definition == r.Definition || strcmp(l.Definition, r.Definition) == 0) {
               return false;
            }
         }

         return true;
      }

      int NDefines() const {
         return empty() ? 0 : (int)size() - 1;
      }
   };

   enum class ShaderType {
      Vertex,
      Hull,
      Domain,
      Pixel,
      Compute,
      Unknown
   };

   struct ShaderDesc {
      std::string path;
      std::string entryPoint;
      ShaderType type = ShaderType::Unknown;
      ShaderDefines defines;

      ShaderDesc() = default;
      ShaderDesc(const string_view path, const string_view& entry_point, ShaderType type) : path(path),
         entryPoint(entry_point), type(type) {}

      friend bool operator==(ShaderDesc const& lhs, ShaderDesc const& rhs) = default;
   };

   class Shader : public RefCounted {
   public:
      Shader(const ShaderDesc& desc);

      bool Valid() const { return compiled; }

      bool compiled = false;
      ShaderDesc desc;

      ComPtr<ID3DBlob> blob;

      ComPtr<ID3D11VertexShader> vs;
      ComPtr<ID3D11HullShader> hs;
      ComPtr<ID3D11DomainShader> ds;
      ComPtr<ID3D11PixelShader> ps;
      ComPtr<ID3D11ComputeShader> cs;

      std::unordered_map<size_t, D3D11_SHADER_INPUT_BIND_DESC> reflection;

      bool Compile(bool force = false);
   };

   struct ProgramDesc {
      ShaderDesc vs;
      ShaderDesc hs;
      ShaderDesc ds;
      ShaderDesc ps;

      ShaderDesc cs;

      friend bool operator==(ProgramDesc const& lhs, ProgramDesc const& rhs) = default;

      // todo:
      // ShaderDefines defines;

      static ProgramDesc Cs(std::string_view path, std::string_view entry) {
         ProgramDesc desc;
         desc.cs = { path.data(), entry.data(), ShaderType::Compute };
         return desc;
      }

      static ProgramDesc VsPs(std::string_view path, std::string_view vsEntry, std::string_view psEntry = {}) {
         ProgramDesc desc;
         desc.vs = { path.data(), vsEntry.data(), ShaderType::Vertex };
         if (!psEntry.empty()) {
            desc.ps = { path.data(), psEntry.data(), ShaderType::Pixel };
         }
         return desc;
      }

      static ProgramDesc VsHsDsPs(std::string_view path, std::string_view vsEntry, std::string_view hsEntry, std::string_view dsEntry, std::string_view psEntry = {}) {
         ProgramDesc desc;
         desc.vs = { path.data(), vsEntry.data(), ShaderType::Vertex };
         desc.hs = { path.data(), hsEntry.data(), ShaderType::Hull };
         desc.ds = { path.data(), dsEntry.data(), ShaderType::Domain };
         if (!psEntry.empty()) {
            desc.ps = { path.data(), psEntry.data(), ShaderType::Pixel };
         }
         return desc;
      }
   };

   class GpuProgram : public RefCounted {
   public:
      static Ref<GpuProgram> Create(const ProgramDesc& desc);

      void Activate(CommandList& cmd);

      template<typename T>
      void SetCB(CommandList& cmd, std::string_view name, Buffer& buffer, uint offsetInBytes = 0) {
         SetCB(cmd, name, buffer, offsetInBytes, sizeof(T));
      }
      void SetCB(CommandList& cmd, std::string_view name, Buffer& buffer, uint offsetInBytes, uint size);

      void SetSRV(CommandList& cmd, std::string_view name, ID3D11ShaderResourceView* srv);
      void SetSRV(CommandList& cmd, std::string_view name, GPUResource& resource);

      void SetUAV(CommandList& cmd, std::string_view name, ID3D11UnorderedAccessView* uav);
      void SetUAV(CommandList& cmd, std::string_view name, GPUResource& resource);

      void DrawInstanced(CommandList& cmd, uint vertCount, uint instCount = 1, uint startVert = 0);
      void DrawIndexedInstanced(CommandList& cmd, uint indexCount, uint instCount = 1, uint indexStart = 0, uint startVert = 0);
      void DrawIndexedInstancedIndirect(CommandList& cmd, Buffer& args, uint offset);

      void Dispatch3D(CommandList& cmd, int3 groups);
      void Dispatch3D(CommandList& cmd, int3 size, int3 groupSize) {
         auto groups = glm::ceil(vec3{ size } / vec3{ groupSize });
         Dispatch3D(cmd, int3{ groups });
      }

      void Dispatch2D(CommandList& cmd, int2 groups) {
         Dispatch3D(cmd, int3{groups, 1});
      }
      void Dispatch2D(CommandList& cmd, int2 size, int2 groupSize) {
         auto groups = glm::ceil(vec2{ size } / vec2{ groupSize });
         Dispatch3D(cmd, int3{ groups, 1 });
      }

      void Dispatch1D(CommandList& cmd, uint size) {
         Dispatch3D(cmd, int3{ size, 1, 1 });
      }

      bool Valid() const;

      Ref<Shader> vs;
      Ref<Shader> hs;
      Ref<Shader> ds;
      Ref<Shader> ps;
      Ref<Shader> cs;

      ProgramDesc desc;

   private:
      friend Ref<GpuProgram>;

      GpuProgram(const ProgramDesc& desc);
   };

   // class GraphicsProgram : public GpuProgram {
   // public:
   // };
   //
   // class ComputeProgram : public GpuProgram {
   // public:
   // };

   GpuProgram* GetGpuProgram(const ProgramDesc& desc);
   void TermGpuPrograms();

   extern CORE_API std::vector<Shader*> sShaders;

   void CORE_API ShadersWindow();
   void CORE_API ShadersSrcWatcherUpdate();

}
