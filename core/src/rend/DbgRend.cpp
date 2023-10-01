#include "pch.h"
#include "DbgRend.h"

#include "scene/Entity.h"
#include "scene/Component.h"
#include "RendRes.h"
#include "Shader.h"
#include "CommandList.h"
#include "math/Shape.h"


namespace pbe {

   DbgRend::DbgRend() {

   }

   DbgRend::~DbgRend() {
   }

   void DbgRend::DrawLine(const vec3& start, const vec3& end, const Color& color, bool zTest, EntityID entityID) {
      uint entityIDUint = (uint)entityID;
      if (zTest) {
         lines.emplace_back(start, entityIDUint, color);
         lines.emplace_back(end, entityIDUint, color);
      } else {
         linesNoZ.emplace_back(start, entityIDUint, color);
         linesNoZ.emplace_back(end, entityIDUint, color);
      }
   }

   void DbgRend::DrawSphere(const Sphere& sphere, const Color& color, bool zTest) {
      // Icosahedron
      const double t = (1.0 + std::sqrt(5.0)) / 2.0;

      std::vector<vec3> points;

      // Vertices
      points.emplace_back(normalize(vec3(-1.0, t, 0.0)));
      points.emplace_back(normalize(vec3(1.0, t, 0.0)));
      points.emplace_back(normalize(vec3(-1.0, -t, 0.0)));
      points.emplace_back(normalize(vec3(1.0, -t, 0.0)));
      points.emplace_back(normalize(vec3(0.0, -1.0, t)));
      points.emplace_back(normalize(vec3(0.0, 1.0, t)));
      points.emplace_back(normalize(vec3(0.0, -1.0, -t)));
      points.emplace_back(normalize(vec3(0.0, 1.0, -t)));
      points.emplace_back(normalize(vec3(t, 0.0, -1.0)));
      points.emplace_back(normalize(vec3(t, 0.0, 1.0)));
      points.emplace_back(normalize(vec3(-t, 0.0, -1.0)));
      points.emplace_back(normalize(vec3(-t, 0.0, 1.0)));

      for (auto& point : points) {
         point *= sphere.radius;
         point += sphere.center;
      }

      auto draw = [&](int a, int b, int c) {
         DrawLine(points[a], points[b], color, zTest);
         DrawLine(points[b], points[c], color, zTest);
         DrawLine(points[c], points[a], color, zTest);
      };

      // Faces
      draw(0, 11, 5);
      draw(0, 5, 1);
      draw(0, 1, 7);
      draw(0, 7, 10);
      draw(0, 10, 11);
      draw(1, 5, 9);
      draw(5, 11, 4);
      draw(11, 10, 2);
      draw(10, 7, 6);
      draw(7, 1, 8);
      draw(3, 9, 4);
      draw(3, 4, 2);
      draw(3, 2, 6);
      draw(3, 6, 8);
      draw(3, 8, 9);
      draw(4, 9, 5);
      draw(2, 4, 11);
      draw(6, 2, 10);
      draw(8, 6, 7);
      draw(9, 8, 1);
   }

   void DbgRend::DrawAABB(const Transform* trans, const AABB& aabb, const Color& color, bool zTest) {
      vec3 a = aabb.min;
      vec3 b = aabb.max;

      vec3 points[8];

      points[0] = points[1] = points[2] = points[3] = a;

      points[1].x = b.x;
      points[2].z = b.z;
      points[3].x = b.x;
      points[3].z = b.z;

      points[4] = points[0];
      points[5] = points[1];
      points[6] = points[2];
      points[7] = points[3];

      points[4].y = b.y;
      points[5].y = b.y;
      points[6].y = b.y;
      points[7].y = b.y;

      if (trans) {
         for (int i = 0; i < 8; ++i) {
            points[i] = trans->TransformPoint(points[i]);
         }
      }

      DrawAABBOrderPoints(points, color, zTest);
   }

   void DbgRend::DrawAABBOrderPoints(const vec3 points[8], const Color& color, bool zTest) {
      DrawLine(points[0], points[1], color, zTest);
      DrawLine(points[0], points[2], color, zTest);
      DrawLine(points[1], points[3], color, zTest);
      DrawLine(points[2], points[3], color, zTest);

      DrawLine(points[4], points[5], color, zTest);
      DrawLine(points[4], points[6], color, zTest);
      DrawLine(points[5], points[7], color, zTest);
      DrawLine(points[6], points[7], color, zTest);

      DrawLine(points[0], points[4], color, zTest);
      DrawLine(points[1], points[5], color, zTest);
      DrawLine(points[2], points[6], color, zTest);
      DrawLine(points[3], points[7], color, zTest);
   }

   void DbgRend::DrawViewProjection(const mat4& invViewProjection, const Color& color, bool zTest) {
      vec4 points[8];

      points[0] = invViewProjection * vec4{-1, -1, 0, 1};
      points[1] = invViewProjection * vec4{1, -1, 0, 1};
      points[2] = invViewProjection * vec4{-1, 1, 0, 1};
      points[3] = invViewProjection * vec4{1, 1, 0, 1};

      points[4] = invViewProjection * vec4{ -1, -1, 1, 1 };
      points[5] = invViewProjection * vec4{ 1, -1, 1, 1 };
      points[6] = invViewProjection * vec4{ -1, 1, 1, 1 };
      points[7] = invViewProjection * vec4{ 1, 1, 1, 1 };

      vec3 points3[8];
      for (int i = 0; i < ARRAYSIZE(points); ++i) {
         points[i] /= points[i].w;
         points3[i] = points[i];
      }

      DrawAABBOrderPoints(points3, color, zTest);
   }

   void DbgRend::DrawFrustum(const Frustum& frustum, const vec3& pos, const vec3& forward, const Color& color, bool zTest) {
      Color colors[6] = { Color_White, Color_Black, Color_Red, Color_Green, Color_Blue, Color_Yellow };

      for (int i = 0; i < 6; ++i) {
         const auto& outNormal = -frustum.planes[i].normal;
         auto start = pos + forward * 2.f + outNormal;
         DrawLine(start, start + outNormal, colors[i], zTest);
      }
   }

   void DbgRend::DrawLine(const Entity& entity0, const Entity& entity1, const Color& color, bool zTest) {
      if (!entity0 || !entity1) {
         return;
      }

      DrawLine(entity0.GetTransform().Position(), entity1.GetTransform().Position(), color, zTest);
   }

   void DbgRend::Clear() {
      lines.clear();
      linesNoZ.clear();
   }

   void DbgRend::Render(CommandList& cmd, const RenderCamera& camera) {
      auto programDesc = ProgramDesc::VsGsPs("dbgRend.hlsl", "vs_main", "MainGS", "ps_main");
      auto program = GetGpuProgram(programDesc);

      program->Activate(cmd);

      auto context = cmd.pContext;

      // cmd.SetBlendState(rendres::blendStateDefaultRGB);
      cmd.SetBlendState(rendres::blendStateTransparency);

      context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

      ID3D11InputLayout* inputLayout = rendres::GetInputLayout(program->vs->blob.Get(), VertexPosUintColor::inputElementDesc);
      context->IASetInputLayout(inputLayout);

      auto draw = [&cmd, &context, &program](std::vector<VertexPosUintColor>& lines) {
         auto stride = (uint)sizeof(VertexPosUintColor);
         auto dynVerts = cmd.AllocDynVertBuffer(lines.data(), stride * (uint)lines.size());

         ID3D11Buffer* vBuffer = dynVerts.buffer->GetBuffer();
         context->IASetVertexBuffers(0, 1, &vBuffer, &stride, &dynVerts.offset);

         program->DrawInstanced(cmd, (uint)lines.size());
      };

      cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadNoWrite);
      draw(lines);

      cmd.SetDepthStencilState(rendres::depthStencilStateNo);
      draw(linesNoZ);
   }

}
