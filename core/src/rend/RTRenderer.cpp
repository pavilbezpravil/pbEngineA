#include "pch.h"
#include "RTRenderer.h"
#include "Buffer.h"
#include "CommandList.h"
#include "DbgRend.h"
#include "Renderer.h" // todo:
#include "Shader.h"
#include "Texture2D.h"
#include "core/CVar.h"
#include "core/Profiler.h"
#include "math/Random.h"

#include "scene/Component.h"
#include "scene/Scene.h"

#include "shared/hlslCppShared.hlsli"
#include "shared/rt.hlsli"


namespace pbe {

   CVarSlider<int> cvNRays{ "render/rt/nRays", 1, 1, 32 };
   CVarSlider<int> cvRayDepth{ "render/rt/rayDepth", 2, 1, 8 };

   CVarValue<bool> cvAccumulate{ "render/rt/accumulate", false };
   CVarValue<bool> cvHistoryReprojection{ "render/rt/history reprojection", false };
   CVarValue<bool> cvImportanceSampling{ "render/rt/importance sampling", false };
   CVarValue<bool> cvDenoise{ "render/rt/denoise", true };
   CVarValue<bool> cvBvhAABBRender{ "render/rt/bvh aabb render", false };
   CVarTrigger cvClearHistory{ "render/rt/clear history"};

   CVarSlider<float> cvReprojectionHistoryWeight{ "render/rt/reprojection/history weight", 0.5f, 0, 1 };
   CVarValue<bool> cvReprojectionShowNewPixel{ "render/rt/reprojection/show new pixel", false };
   CVarValue<bool> cvReprojectionObjID{ "render/rt/reprojection/obj id", true };
   CVarValue<bool> cvReprojectionNormal{ "render/rt/reprojection/normal", true };
   CVarValue<bool> cvReprojectionDepth{ "render/rt/reprojection/depth", true };

   // todo: move to common
   static int IndexOfLargestValue(const vec3& vector) {
      int index = 0;
      float maxValue = vector[0];

      for (int i = 1; i < 3; ++i) {
         if (vector[i] > maxValue) {
            index = i;
            maxValue = vector[i];
         }
      }

      return index;
   }

   void RTRenderer::Init() {

   }

   struct BVH {
      struct Node {
         vec3 aabbMin;
         uint objIdx = UINT_MAX;

         vec3 aabbMax;
         uint pad;
      };

      std::vector<Node> nodes;

      std::vector<Node> buildedNodes;

      int Nodes() const {
         return (int)buildedNodes.size();
      }

      void Build(const std::vector<AABB>& aabbs) {
         nodes.clear();
         buildedNodes.clear();

         int size = (int)aabbs.size();
         if (size == 0) {
            return;
         }

         // todo: do it thought generator
         nodes.resize(size);

         for (int i = 0; i < size; ++i) {
            nodes[i] = Node {
               .aabbMin = aabbs[i].min,
               .objIdx = (uint)i,
               .aabbMax = aabbs[i].max,
            };
         }

         // todo: not best size
         buildedNodes.reserve(size* 2);
         BuildRecursive(0, 0, size);
      }

      void Render(DbgRend& dbgRend, int idx = 0, int level = 0) {
         if (idx >= buildedNodes.size()) {
            return;
         }

         auto& node = buildedNodes[idx];

         vec3 color = Random::Color(level);
         dbgRend.DrawAABB(AABB::MinMax(node.aabbMin, node.aabbMax), vec4(color, 1));

         if (node.objIdx == UINT_MAX) {
            Render(dbgRend, LeftIdx(idx), level + 1);
            Render(dbgRend, RightIdx(idx), level + 1);
         }
      }

   private:
      void BuildRecursive(int buildNodeIdx, int start, int end) {
         int count = end - start;
         if (count == 0) {
            return;
         }

         AABB combinedAABB = AABB::MinMax(nodes[start].aabbMin, nodes[start].aabbMax);
         for (int i = 1; i < count; ++i) {
            auto& node = nodes[start + i];
            combinedAABB.AddAABB(AABB::MinMax(node.aabbMin, node.aabbMax));
         }

         int mid = start + (count + 1) / 2;

         if (count > 2) {
            int axis = IndexOfLargestValue(combinedAABB.Size());
            std::sort(nodes.begin() + start, nodes.begin() + end,
               [axis](const Node& a, const Node& b) { return a.aabbMin[axis] < b.aabbMin[axis]; });
         } else {
            mid = start + 1;
         }

         int reqSize = buildNodeIdx + 1;
         if (reqSize > buildedNodes.size()) {
            buildedNodes.resize(reqSize);
         }

         buildedNodes[buildNodeIdx] = Node {
            .aabbMin = combinedAABB.min,
            .objIdx = count == 1 ? nodes[start].objIdx : UINT_MAX,
            .aabbMax = combinedAABB.max,
         };

         if (count > 1) {
            BuildRecursive(LeftIdx(buildNodeIdx), start, mid);
            BuildRecursive(RightIdx(buildNodeIdx), mid, end);
         }
      }

      int ParentIdx(int idx) const {
         return idx == 0 ? 0 : (idx - 1) / 2;
      }

      int LeftIdx(int idx) const {
         return idx * 2 + 1;
      }

      int RightIdx(int idx) const {
         return idx * 2 + 2;
      }
   };

   void RTRenderer::RenderScene(CommandList& cmd, const Scene& scene, const RenderCamera& camera, RenderContext& context) {
      GPU_MARKER("RT Scene");
      PROFILE_GPU("RT Scene");

      uint importanceSampleObjIdx = -1;

      std::vector<SRTObject> objs;

      std::vector<AABB> aabbs;

      for (auto [e, trans, material, geom]
         : scene.View<SceneTransformComponent, MaterialComponent, GeometryComponent>().each()) {
         auto rotation = trans.Rotation();
         auto position = trans.Position();
         auto scale = trans.Scale();

         SRTObject obj;
         obj.position = position;
         obj.id = (uint)e;

         obj.rotation = glm::make_vec4(glm::value_ptr(rotation));

         obj.geomType = (int)geom.type;
         obj.halfSize = geom.sizeData / 2.f * scale;

         obj.baseColor = material.baseColor;
         obj.metallic = material.metallic;
         obj.roughness = material.roughness;
         obj.emissiveColor = material.emissiveColor * material.emissivePower;
         if (material.emissivePower > 0) {
            importanceSampleObjIdx = (uint)objs.size();
         }

         // todo: sphere may be optimized
         // todo: not fastest way

         auto extends = scale * 0.5f;
         auto aabb = AABB::Empty();

         if (geom.type == GeomType::Box) {
            aabb.AddPoint(rotation * extends);
            aabb.AddPoint(rotation * vec3{ -extends.x, extends.y, extends.z });
            aabb.AddPoint(rotation * vec3{ extends.x, -extends.y, extends.z });
            aabb.AddPoint(rotation * vec3{ extends.x, extends.y, -extends.z });

            vec3 absMax = glm::max(abs(aabb.min), abs(aabb.max));
            aabb.min = -absMax;
            aabb.max = absMax;

            aabb.Translate(position);
         } else {
            auto sphereExtends = vec3{ extends.x };
            aabb = AABB::Extends(trans.Position(), sphereExtends);
         }

         aabbs.emplace_back(aabb);

         objs.emplace_back(obj);
      }

      // todo: dont create every frame
      BVH bvh;
      bvh.Build(aabbs);

      if (cvBvhAABBRender) {
         auto& dbgRender = *scene.dbgRend;
         // for (auto& aabb : aabbs) {
         //    dbgRender.DrawAABB(aabb);
         // }
         bvh.Render(dbgRender);
      }

      // todo: make it dynamic
      int bvhNodes = bvh.Nodes();
      if (!bvhNodesBuffer || bvhNodesBuffer->ElementsCount() < bvhNodes) {
         auto bufferDesc = Buffer::Desc::Structured("BVHNodes", bvhNodes, sizeof(BVHNode));
         bvhNodesBuffer = Buffer::Create(bufferDesc);
      }
      cmd.UpdateSubresource(*bvhNodesBuffer, bvh.buildedNodes.data(), 0, bvhNodes * sizeof(BVHNode));

      uint nObj = (uint)objs.size();

      // todo: make it dynamic
      if (!rtObjectsBuffer || rtObjectsBuffer->ElementsCount() < nObj) {
         auto bufferDesc = Buffer::Desc::Structured("RtObjects", nObj, sizeof(SRTObject));
         rtObjectsBuffer = Buffer::Create(bufferDesc);
      }

      cmd.UpdateSubresource(*rtObjectsBuffer, objs.data(), 0, nObj * sizeof(SRTObject));

      auto& outTexture = *context.colorHDR;
      auto outTexSize = outTexture.GetDesc().size;

      // cmd.ClearRenderTarget(*context.colorHDR, vec4{ 0, 0, 0, 1 });

      static int accumulatedFrames = 0;

      bool resetHistory = cvClearHistory;

      float historyWeight = cvReprojectionHistoryWeight;

      if (cvAccumulate) {
         historyWeight = (float)accumulatedFrames / (float)(accumulatedFrames + cvNRays);
         accumulatedFrames += cvNRays;

         resetHistory |= camera.prevViewProjection != camera.GetViewProjection();
      } else {
         accumulatedFrames = 0;
      }

      if (resetHistory || !cvAccumulate) {
         // cmd.ClearUAVUint(*context.reprojectCountTexPrev);
         // cmd.ClearUAVFloat(*context.historyTex);

         accumulatedFrames = 0;
      }

      SRTConstants rtCB;
      rtCB.rtSize = outTexSize;
      rtCB.rayDepth = cvRayDepth;
      rtCB.nObjects = nObj;
      rtCB.nRays = cvNRays;
      rtCB.random01 = Random::Float(0.f, 1.f);
      rtCB.historyWeight = historyWeight;
      rtCB.importanceSampleObjIdx = importanceSampleObjIdx;

      rtCB.debugFlags = 0;
      rtCB.debugFlags |= cvReprojectionShowNewPixel ? DBG_FLAG_SHOW_NEW_PIXEL : 0;
      rtCB.debugFlags |= cvReprojectionObjID ? DBG_FLAG_REPR_OBJ_ID : 0;
      rtCB.debugFlags |= cvReprojectionNormal ? DBG_FLAG_REPR_NORMAL : 0;
      rtCB.debugFlags |= cvReprojectionDepth ? DBG_FLAG_REPR_DEPTH : 0;

      rtCB.bvhNodes = bvhNodes;

      auto rtConstantsCB = cmd.AllocDynConstantBuffer(rtCB);

      auto setSharedResource = [&](GpuProgram& pass) {
         cmd.SetCB<SRTConstants>(pass.GetBindPoint("gRTConstantsCB"), rtConstantsCB.buffer, rtConstantsCB.offset);
         cmd.SetSRV(pass.GetBindPoint("gRtObjects"), rtObjectsBuffer);
         cmd.SetSRV(pass.GetBindPoint("gBVHNodes"), bvhNodesBuffer);
      };

      CMD_BINDS_GUARD();

      // todo: generate by rasterizator
      if (cvDenoise && false) {
         GPU_MARKER("GBuffer");
         PROFILE_GPU("GBuffer");

         auto pass = GetGpuProgram(ProgramDesc::Cs("rt.hlsl", "GBufferCS"));
         cmd.SetCompute(*pass);
         setSharedResource(*pass);

         CMD_BINDS_GUARD();

         // cmd.SetUAV(pass->GetBindPoint("gDepthOut"), context.depthTex);
         cmd.SetUAV(pass->GetBindPoint("gNormalOut"), context.normalTex);

         cmd.SetUAV(pass->GetBindPoint("gUnderCursorBuffer"), context.underCursorBuffer); // todo: only for editor

         cmd.Dispatch2D(outTexSize, int2{ 8, 8 });
      }

      {
         GPU_MARKER("Ray Trace");
         PROFILE_GPU("Ray Trace");

         auto desc = ProgramDesc::Cs("rt.hlsl", "rtCS");
         // if (cvImportanceSampling) {
         //    desc.cs.defines.AddDefine("IMPORTANCE_SAMPLING");
         // }

         auto pass = GetGpuProgram(desc);
         cmd.SetCompute(*pass);
         setSharedResource(*pass);

         CMD_BINDS_GUARD();

         cmd.SetUAV(pass->GetBindPoint("gColorOut"), context.rtColorNoisyTex);

         cmd.Dispatch2D(outTexSize, int2{8, 8});
      }

      if (cvAccumulate && !cvDenoise) {
         UNIMPLEMENTED();

         // todo: incorrect name
         GPU_MARKER("Reproject");
         PROFILE_GPU("Reproject");

         auto desc = ProgramDesc::Cs("rt.hlsl", "HistoryAccCS");
         if (cvHistoryReprojection) {
            desc.cs.defines.AddDefine("ENABLE_REPROJECTION");
         }

         auto pass = GetGpuProgram(desc);
         cmd.SetCompute(*pass);
         setSharedResource(*pass);

         CMD_BINDS_GUARD();

         cmd.SetSRV(pass->GetBindPoint("gReprojectCount"), context.reprojectCountTexPrev);
         // cmd.SetSRV(pass->GetBindPoint("gDepthPrev"), context.depthTexPrev);
         // cmd.SetSRV(pass->GetBindPoint("gDepth"), context.depthTex);
         // cmd.SetSRV(pass->GetBindPoint("gNormalPrev"), context.normalTexPrev);
         cmd.SetSRV(pass->GetBindPoint("gNormal"), context.normalTex);
         cmd.SetSRV(pass->GetBindPoint("gHistory"), context.historyTexPrev);

         cmd.SetUAV(pass->GetBindPoint("gReprojectCountOut"), context.reprojectCountTex);
         cmd.SetUAV(pass->GetBindPoint("gColor"), context.colorHDR);
         cmd.SetUAV(pass->GetBindPoint("gHistoryOut"), context.historyTex);

         cmd.Dispatch2D(outTexSize, int2{ 8, 8 });

         // if (cvHistoryReprojection) {
         //    std::swap(context.historyTex, context.historyTexPrev);
         //    std::swap(context.reprojectCountTex, context.reprojectCountTexPrev);
         //    std::swap(context.normalTex, context.normalTexPrev);
         //    std::swap(context.depthTex, context.depthTexPrev);
         // }
      }

      if (cvDenoise) {
         GPU_MARKER("Denoise");
         PROFILE_GPU("Denoise");

         // cvHistoryReprojection&& cvAccumulate

         auto desc = ProgramDesc::Cs("rt.hlsl", "DenoiseCS");
         auto pass = GetGpuProgram(desc);

         cmd.SetCompute(*pass);
         setSharedResource(*pass);

         CMD_BINDS_GUARD();

         // cmd.SetSRV(pass->GetBindPoint("gReprojectCount"), context.reprojectCountTexPrev);
         // cmd.SetSRV(pass->GetBindPoint("gDepth"), context.depthTex);
         cmd.SetSRV(pass->GetBindPoint("gDepth"), context.depth);
         cmd.SetSRV(pass->GetBindPoint("gNormal"), context.normalTex);
         cmd.SetSRV(pass->GetBindPoint("gColor"), context.rtColorNoisyTex);
         cmd.SetSRV(pass->GetBindPoint("gHistory"), context.historyTex);

         cmd.SetUAV(pass->GetBindPoint("gHistoryOut"), context.historyTexPrev); // todo: not prev
         cmd.SetUAV(pass->GetBindPoint("gColorOut"), context.colorHDR);

         cmd.Dispatch2D(outTexSize, int2{ 8, 8 });

         std::swap(context.historyTex, context.historyTexPrev);
         std::swap(context.reprojectCountTex, context.reprojectCountTexPrev);
      }
   }

}
