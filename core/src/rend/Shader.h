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
   class Buffer;

   class Texture2D;
   class CommandList;

   void ReloadShaders();

   struct ShaderDefine {
      std::string define;
      std::string value;
   };

   // todo: inherit from vector
   using ShaderDefines = std::vector<ShaderDefine>;

   enum class ShaderType {
      Vertex,
      Pixel,
      Compute,
      Unknown
   };

   struct ShaderDesc {
      std::string path;
      std::string entryPoint;
      ShaderType type = ShaderType::Unknown;

      ShaderDesc() = default;
      ShaderDesc(const std::string& path, const std::string& entry_point, ShaderType type) : path(path),
         entryPoint(entry_point), type(type) {}
   };

   class Shader : public RefCounted {
   public:
      Shader(ShaderDesc& desc);

      bool compiled = false;
      ShaderDesc desc;

      ComPtr<ID3DBlob> blob;

      ComPtr<ID3D11VertexShader> vs;
      ComPtr<ID3D11PixelShader> ps;
      ComPtr<ID3D11ComputeShader> cs;

      std::unordered_map<size_t, D3D11_SHADER_INPUT_BIND_DESC> reflection;

      bool Compile();
   };

   struct ProgramDesc {
      ShaderDesc vs;
      ShaderDesc ps;
      ShaderDesc cs;

      // todo:
      // ShaderDefines defines;

      static ProgramDesc Cs(std::string_view path, std::string_view entry) {
         ProgramDesc desc;
         desc.cs = { path.data(), entry.data(), ShaderType::Compute };
         return desc;
      }

      static ProgramDesc VsPs(std::string_view path, std::string_view vsEntry, std::string_view psEntry) {
         ProgramDesc desc;
         desc.vs = { path.data(), vsEntry.data(), ShaderType::Vertex };
         desc.ps = { path.data(), psEntry.data(), ShaderType::Pixel };
         return desc;
      }
   };

   class GpuProgram : public RefCounted {
   public:
      static Ref<GpuProgram> Create(ProgramDesc& desc);

      void Activate(CommandList& cmd);

      void SetConstantBufferRaw(CommandList& cmd, std::string_view name, std::span<byte> data);
      void SetConstantBuffer(CommandList& cmd, std::string_view name, Buffer& buffer);
      void SetSrvBuffer(CommandList& cmd, std::string_view name, Buffer& buffer);
      void SetTexture(CommandList& cmd, std::string_view name, Texture2D& texture);

      void DrawInstanced(CommandList& cmd, int vertCount, int instCount = 1, int startVert = 0);
      void DrawIndexedInstanced(CommandList& cmd, int indexCount, int instCount = 1, int indexStart = 0, int startVert = 0);
      void Dispatch(CommandList& cmd, int3 groups);

      Ref<Shader> vs;
      Ref<Shader> ps;
      Ref<Shader> cs;

      ProgramDesc desc;

   private:
      friend Ref<GpuProgram>;

      GpuProgram(ProgramDesc& desc);
   };

   // class GraphicsProgram : public GpuProgram {
   // public:
   // };
   //
   // class ComputeProgram : public GpuProgram {
   // public:
   // };

}
