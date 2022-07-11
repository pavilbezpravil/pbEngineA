#pragma once
#include <d3d11shader.h>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Core.h"
#include "core/Ref.h"
#include "math/Types.h"

struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11ComputeShader;

class Texture2D;
class CommandList;

void ShaderCompileTest();

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
   // todo: leak
   ID3D11VertexShader* vs{};
   ID3D11PixelShader* ps{};
   ID3D11ComputeShader* cs{};

   std::unordered_map<size_t, D3D11_SHADER_INPUT_BIND_DESC> reflection;
};

struct ProgramDesc {
   ShaderDesc vs;
   ShaderDesc ps;
   ShaderDesc cs;

   // todo:
   // ShaderDefines defines;

   static ProgramDesc Cs(std::string_view path, std::string_view entry) {
      ProgramDesc desc;
      desc.cs = {path.data(), entry.data(), ShaderType::Compute};
      return desc;
   }

   static ProgramDesc VsPs(std::string_view path, std::string_view vsEntry, std::string_view psEntry) {
      ProgramDesc desc;
      desc.vs = { path.data(), vsEntry.data(), ShaderType::Vertex };
      desc.ps = { path.data(), psEntry.data(), ShaderType::Pixel };
      return desc;
   }
};

class GpuProgram {
public:
   GpuProgram(ProgramDesc& desc);

   void SetConstantBufferRaw(CommandList& cmd, std::string_view name, std::span<byte> data);
   void SetTexture(CommandList& cmd, std::string_view name, Texture2D& texture);

   void Draw(CommandList& cmd);
   void Dispatch(CommandList& cmd, int3 groups);

   Ref<Shader> vs;
   Ref<Shader> ps;
   Ref<Shader> cs;

   ProgramDesc desc;
};

// class GraphicsProgram : public GpuProgram {
// public:
// };
//
// class ComputeProgram : public GpuProgram {
// public:
// };
