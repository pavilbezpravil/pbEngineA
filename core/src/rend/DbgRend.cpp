#include "pch.h"
#include "DbgRend.h"

#include "RendRes.h"
#include "Shader.h"
#include "CommandList.h"
#include "Renderer.h"
#include "math/Shape.h"

#include "shared/common.hlsli"


namespace pbe {

   DbgRend::DbgRend() {
      auto programDesc = ProgramDesc::VsPs("dbgRend.hlsl", "vs_main", "ps_main");
      program = GpuProgram::Create(programDesc);
   }

   void DbgRend::DrawLine(const vec3& start, const vec3& end, const vec4& color) {
      lines.emplace_back(start, color);
      lines.emplace_back(end, color);
   }

   void DbgRend::DrawAABB(const AABB& aabb, const vec4& color) {
      vec3 a = aabb.min;
      vec3 b = aabb.max;

      vec3 points[8];

      points[0] = points[1] = points[2] = points[3] = a;

      points[1].x = b.x;
      points[2].z = b.z;
      points[3].x = b.x;
      points[3].z = b.z;

      DrawLine(points[0], points[1], color);
      DrawLine(points[0], points[2], color);
      DrawLine(points[1], points[3], color);
      DrawLine(points[2], points[3], color);

      points[4] = points[0];
      points[5] = points[1];
      points[6] = points[2];
      points[7] = points[3];

      points[4].y = b.y;
      points[5].y = b.y;
      points[6].y = b.y;
      points[7].y = b.y;

      DrawLine(points[4], points[5], color);
      DrawLine(points[4], points[6], color);
      DrawLine(points[5], points[7], color);
      DrawLine(points[6], points[7], color);

      DrawLine(points[0], points[4], color);
      DrawLine(points[1], points[5], color);
      DrawLine(points[2], points[6], color);
      DrawLine(points[3], points[7], color);
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
