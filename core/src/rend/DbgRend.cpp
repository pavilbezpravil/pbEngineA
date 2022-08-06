#include "pch.h"
#include "DbgRend.h"

#include "RendRes.h"
#include "Shader.h"
#include "CommandList.h"
#include "Renderer.h"

#include "shared/common.hlsli"


namespace pbe {

   DbgRend::DbgRend() {
      auto programDesc = ProgramDesc::VsPs("dbgRend.hlsl", "vs_main", "ps_main");
      program = GpuProgram::Create(programDesc);
   }

   void DbgRend::Line(const vec3& start, const vec3& end, const vec4& color) {
      lines.emplace_back(start, color);
      lines.emplace_back(end, color);
   }

   void DbgRend::Clear() {
      lines.clear();
   }

   void DbgRend::Render(CommandList& cmd, const RenderCamera& camera) {
      auto context = cmd.pContext;

      cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadNoWrite);
      cmd.SetBlendState(rendres::blendStateDefaultRGB);
      // cmd.SetBlendState(rendres::blendStateTransparency);

      context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

      ID3D11InputLayout* inputLayout = rendres::GetInputLayout(program->vs->blob.Get(), VertexPosColor::inputElementDesc);
      context->IASetInputLayout(inputLayout);

      auto stride = (uint)sizeof(VertexPosColor);
      auto dynVerts = cmd.AllocDynVertBuffer(lines.data(), stride * lines.size());

      ID3D11Buffer* vBuffer = dynVerts.buffer->GetBuffer();
      context->IASetVertexBuffers(0, 1, &vBuffer, &stride, &dynVerts.offset);

      SCameraCB cameraCB;
      camera.FillSCameraCB(cameraCB);

      auto dynCameraCB = cmd.AllocDynConstantBuffer(cameraCB);

      program->Activate(cmd);
      program->SetCB<SCameraCB>(cmd, "gCameraCB", *dynCameraCB.buffer, dynCameraCB.offset);
      program->DrawInstanced(cmd, (uint)lines.size());
   }

}
