#include "pch.h"
#include "Terrain.h"

#include "core/CVar.h"
#include "core/Profiler.h"
#include "math/Random.h"
#include "math/Types.h"
#include "rend/CommandList.h"
#include "rend/Renderer.h"
#include "rend/RendRes.h"
#include "rend/Shader.h"
#include "scene/Scene.h"

#include <shared/hlslCppShared.hlsli>

namespace pbe {

#define C_TERRAIN_PATH "terrain/"

   CVarValue<bool> cTerrainDraw{ C_TERRAIN_PATH "draw", true };

   CVarValue<bool> cTerrainWireframe{ C_TERRAIN_PATH "wireframe", false };
   CVarValue<bool> cTerrainPixelNormal{ C_TERRAIN_PATH "pixel normal", true };
   CVarSlider<float> cTerrainTessFactor{ C_TERRAIN_PATH "tess factor", 64.f, 0.f, 128.f };
   CVarSlider<float> cTerrainPatchSize{ C_TERRAIN_PATH "patch size", 4.f, 1.f, 32.f };
   CVarSlider<int> cTerrainPatchCount{ C_TERRAIN_PATH "patch count", 64, 1, 512 };

   void Terrain::Render(CommandList& cmd, Scene& scene, CameraContext& cameraContext) {
      if (!cTerrainDraw) {
         return;
      }

      GPU_MARKER("Terrain");
      PROFILE_GPU("Terrain");

      cmd.SetRenderTargetsUAV(cameraContext.colorHDR, cameraContext.depth, cameraContext.underCursorBuffer);
      cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);
      cmd.SetBlendState(rendres::blendStateDefaultRGB);

      if (cTerrainWireframe) {
         cmd.SetRasterizerState(rendres::rasterizerStateWireframe);
      }

      auto* context = cmd.pContext;
      context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
      context->IASetInputLayout(nullptr);

      auto terrainPass = GetGpuProgram(ProgramDesc::VsHsDsPs("terrain.hlsl", "waterVS", "waterHS", "waterDS", "waterPS"));
      terrainPass->Activate(cmd);

      STerrainCB terrainCB;

      terrainCB.waterTessFactor = cTerrainTessFactor;
      terrainCB.waterPatchSize = cTerrainPatchSize;
      terrainCB.waterPatchCount = cTerrainPatchCount;
      terrainCB.waterPixelNormals = cTerrainPixelNormal;

      for (auto [e, trans, terrain] : scene.View<SceneTransformComponent, TerrainComponent>().each()) {
         terrainCB.center = trans.position;
         terrainCB.color = terrain.color;
         terrainCB.entityID = (uint)e;

         auto dynWaterCB = cmd.AllocDynConstantBuffer(terrainCB);
         terrainPass->SetCB<SWaterCB>(cmd, "gTerrainCB", *dynWaterCB.buffer, dynWaterCB.offset);

         terrainPass->DrawInstanced(cmd, 4, cTerrainPatchCount * cTerrainPatchCount);
      }
   }

}
