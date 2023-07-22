#include "pch.h"
#include "ViewportWindow.h"

#include "EditorWindow.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "Undo.h"
#include "app/Input.h"
#include "core/CVar.h"
#include "rend/Renderer.h"
#include "rend/RendRes.h"
#include "math/Types.h"
#include "typer/Serialize.h"


namespace pbe {

   // todo: add hotkey remove cfg
   CVarValue<bool> zoomEnable{ "zoom/enable", false };
   CVarSlider<float> zoomScale{ "zoom/scale", 0.05f, 0.f, 1.f };

   struct ViewportSettings {
      vec3 cameraPos{};
      vec2 cameraAngles{};
   };

   constexpr char viewportSettingPath[] = "scene_viewport.yaml";

   TYPER_BEGIN(ViewportSettings)
      TYPER_FIELD(cameraPos)
      TYPER_FIELD(cameraAngles)
   TYPER_END()

   ViewportWindow::ViewportWindow(std::string_view name): EditorWindow(name) {
      ViewportSettings viewportSettings;
      Deserialize(viewportSettingPath, viewportSettings);

      camera.position = viewportSettings.cameraPos;
      camera.cameraAngle = viewportSettings.cameraAngles;
      camera.UpdateView();

      renderer.reset(new Renderer());
      renderer->Init();
   }

   ViewportWindow::~ViewportWindow() {
      Serialize(viewportSettingPath, ViewportSettings{.cameraPos = camera.position, .cameraAngles = camera.cameraAngle});
   }

   void ViewportWindow::OnImGuiRender() {
      ImGui::Begin(name.c_str(), &show);

      enableInput = ImGui::IsWindowHovered();

      if (customHeadFunc) {
         customHeadFunc();
      }

      static RenderConfing cfg;

      ImGui::Checkbox("Transparency", &cfg.transparency);
      ImGui::SameLine();
      ImGui::Checkbox("Transparency Sorting", &cfg.transparencySorting);
      ImGui::SameLine();

      ImGui::Checkbox("Opaque Sorting", &cfg.opaqueSorting);
      ImGui::SameLine();

      ImGui::Checkbox("Decals", &cfg.decals);
      ImGui::SameLine();

      ImGui::Checkbox("Shadow", &cfg.useShadowPass);
      ImGui::SameLine();

      ImGui::Checkbox("Use ZPass", &cfg.useZPass);
      ImGui::SameLine();
      ImGui::Checkbox("SSAO", &cfg.ssao);
      ImGui::SameLine();
      ImGui::Checkbox("Use InstancedDraw", &cfg.useInstancedDraw);

      static bool textureViewWindow = false;
      ImGui::Checkbox("Texture View Window", &textureViewWindow);
      ImGui::SameLine();

      ImGui::Checkbox("Super Sampling", &cfg.superSampling);

      renderer->cfg = cfg;

      static int item_current = 0;
      const char* items[] = { "ColorLDR", "ColorHDR", "Depth", "LinearDepth", "Normal", "Position", "SSAO", "ShadowMap", "RT Depth", "RT Normal"};

      ImGui::SetNextItemWidth(80);
      ImGui::Combo("Scene RTs", &item_current, items, IM_ARRAYSIZE(items));

      CommandList cmd{ sDevice->g_pd3dDeviceContext };

      if (selectEntityUnderCursor) {
         auto entityID = renderer->GetEntityIDUnderCursor(cmd);

         bool clearPrevSelection = !Input::IsKeyPressed(VK_CONTROL);
         if (entityID != (uint) -1) {
            Entity e{ entt::entity(entityID), scene};
            selection->ToggleSelect(e, clearPrevSelection);
         } else if (clearPrevSelection) {
            selection->ClearSelection();
         }
         selectEntityUnderCursor = false;
      }

      // todo: hack. lambda with capture cant be passed as function pointer
      static auto sCtx = cmd.pContext;
      auto setPointSampler = [](const ImDrawList* cmd_list, const ImDrawCmd* pcmd) {
         sCtx->PSSetSamplers(0, 1, &rendres::samplerStateWrapPoint);
      };

      auto imSize = ImGui::GetContentRegionAvail();
      int2 size = { imSize.x, imSize.y };
      if (cfg.superSampling) {
         size *= 2;
      }

      if (size.x > 1 && size.y > 1) {
         if (!cameraContext.colorHDR || cameraContext.colorHDR->GetDesc().size != size) {
            Texture2D::Desc texDesc;
            texDesc.size = size;

            texDesc.name = "scene colorHDR";
            texDesc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            // texDesc.format = DXGI_FORMAT_R11G11B10_FLOAT; // my laptop doesnot support this format as UAV
            texDesc.bindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            texDesc.bindFlags |= D3D11_BIND_UNORDERED_ACCESS; // todo:
            cameraContext.colorHDR = Texture2D::Create(texDesc);

            texDesc.name = "scene colorLDR";
            // texDesc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            // texDesc.format = DXGI_FORMAT_R8G8B8A8_UNORM; // todo: test srgb
            cameraContext.colorLDR = Texture2D::Create(texDesc);

            texDesc.name = "water refraction";
            texDesc.bindFlags = D3D11_BIND_SHADER_RESOURCE;
            cameraContext.waterRefraction = Texture2D::Create(texDesc);

            texDesc.name = "scene depth";
            // texDesc.format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            texDesc.format = DXGI_FORMAT_R24G8_TYPELESS;
            texDesc.bindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
            cameraContext.depth = Texture2D::Create(texDesc);

            texDesc.name = "scene depth without water";
            texDesc.bindFlags = D3D11_BIND_SHADER_RESOURCE;
            cameraContext.depthWithoutWater = Texture2D::Create(texDesc);

            texDesc.name = "scene linear depth";
            texDesc.format = DXGI_FORMAT_R16_FLOAT;
            texDesc.mips = 0;
            texDesc.bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
            cameraContext.linearDepth = Texture2D::Create(texDesc);

            texDesc.mips = 1;

            // texDesc.bindFlags = D3D11_BIND_SHADER_RESOURCE;
            // texDesc.name = "scene depth copy";
            // cameraContext.depthCopy = Texture2D::Create(texDesc);

            texDesc.name = "scene normal";
            texDesc.format = DXGI_FORMAT_R16G16B16A16_SNORM;
            texDesc.bindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            cameraContext.normal = Texture2D::Create(texDesc);

            texDesc.name = "scene position";
            texDesc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            texDesc.bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
            cameraContext.position = Texture2D::Create(texDesc);

            texDesc.name = "scene ssao";
            texDesc.format = DXGI_FORMAT_R16_UNORM;
            texDesc.bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
            cameraContext.ssao = Texture2D::Create(texDesc);

            if (!cameraContext.shadowMap) {
               Texture2D::Desc texDesc;
               texDesc.name = "shadow map";
               // texDesc.format = DXGI_FORMAT_D16_UNORM;
               texDesc.format = DXGI_FORMAT_R16_TYPELESS;
               texDesc.size = { 1024, 1024 };
               texDesc.bindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
               cameraContext.shadowMap = Texture2D::Create(texDesc);
            }

            // rt
            {
               auto& outTexture = *cameraContext.colorHDR;
               auto outTexSize = outTexture.GetDesc().size;

               Texture2D::Desc texDesc{
                  .size = outTexSize,
                  .format = outTexture.GetDesc().format,
                  .bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
                  .name = "rt history",
               };
               cameraContext.historyTex = Texture2D::Create(texDesc);
               cameraContext.historyTex2 = Texture2D::Create(texDesc);

               texDesc = {
                  .size = outTexSize,
                  .format = DXGI_FORMAT_R32_FLOAT, // DXGI_FORMAT_R32_TYPELESS, // todo:
                  .bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
                  .name = "rt depth",
               };
               cameraContext.depthTex = Texture2D::Create(texDesc);
               cameraContext.depthTexPrev = Texture2D::Create(texDesc);

               texDesc = {
                  .size = outTexSize,
                  .format = DXGI_FORMAT_R8G8B8A8_UNORM,
                  .bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
                  .name = "rt normal",
               };
               cameraContext.normalTex = Texture2D::Create(texDesc);
               cameraContext.normalTexPrev = Texture2D::Create(texDesc);

               texDesc = {
                  .size = outTexSize,
                  .format = DXGI_FORMAT_R8_UINT,
                  .bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
                  .name = "rt reproject count",
               };
               cameraContext.reprojectCountTex = Texture2D::Create(texDesc);
               cameraContext.reprojectCountTexPrev = Texture2D::Create(texDesc);

               texDesc = {
                  .size = outTexSize,
                  .format = DXGI_FORMAT_R32_UINT,
                  .bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
                  .name = "rt obj id",
               };
               cameraContext.objIDTex = Texture2D::Create(texDesc);
               cameraContext.objIDTexPrev = Texture2D::Create(texDesc);
            }

            camera.UpdateProj(texDesc.size);
            camera.NextFrame(); // todo:
         }

         vec2 mousePos = { ImGui::GetMousePos().x, ImGui::GetMousePos().y };
         vec2 cursorPos = { ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y };

         vec2 pixelIdxFloat = (mousePos - cursorPos);

         // camera.NextFrame(); // todo:

         cmd.SetCommonSamplers();
         if (scene) {
            // todo:
            RenderCamera rendCamera;

            rendCamera.position = camera.position;
            rendCamera.view = camera.view;
            rendCamera.projection = camera.projection;
            rendCamera.prevViewProjection = camera.prevViewProjection;

            rendCamera.zNear = camera.zNear;
            rendCamera.zFar = camera.zFar;


            int2 cursorPixelIdx{ pixelIdxFloat + EPSILON };
            cameraContext.cursorPixelIdx = cursorPixelIdx;
            renderer->RenderScene(cmd, *scene, rendCamera, cameraContext);
         }
         cmd.pContext->ClearState(); // todo:

         Texture2D* sceneRTs[] = { cameraContext.colorLDR, cameraContext.colorHDR, cameraContext.depth, cameraContext.linearDepth, cameraContext.normal, cameraContext.position, cameraContext.ssao, cameraContext.shadowMap, cameraContext.depthTex, cameraContext.normalTex };
         auto srv = sceneRTs[item_current]->srv.Get();

         ImGui::Image(srv, imSize);

         if (zoomEnable) {
            vec2 zoomImageSize{300, 300};

            vec2 uvCenter = pixelIdxFloat / vec2{ imSize.x, imSize.y };

            if (all(uvCenter > vec2{ 0 } && uvCenter < vec2{ 1 })) {
               vec2 uvScale{ zoomScale / 2.f };

               vec2 uv0 = uvCenter - uvScale;
               vec2 uv1 = uvCenter + uvScale;

               ImGui::BeginTooltip();

               ImGui::GetWindowDrawList()->AddCallback(setPointSampler, nullptr);
               ImGui::Image(srv, { zoomImageSize.x, zoomImageSize.y }, { uv0.x, uv0.y }, { uv1.x, uv1.y });
               ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

               ImGui::EndTooltip();
            }
         }

         Gizmo(vec2{ imSize.x, imSize.y }, cursorPos);
      }

      ImGui::End();

      if (textureViewWindow && cameraContext.linearDepth) {
         ImGui::Begin("Texture View");

         auto texture = cameraContext.linearDepth;

         const auto& desc = texture->GetDesc();
         ImGui::Text("%s (%dx%d, %d)", desc.name.c_str(), desc.size.x, desc.size.y, desc.mips);

         static int iMip = 0;
         ImGui::SliderInt("mip", &iMip, 0, texture->GetDesc().mips - 1);
         ImGui::SameLine();

         int d = 1 << iMip;
         ImGui::Text("mip size (%dx%d)", desc.size.x / d, desc.size.y / d);

         auto imSize = ImGui::GetContentRegionAvail();
         auto srv = texture->GetMipSrv(iMip);

         ImGui::GetWindowDrawList()->AddCallback(setPointSampler, nullptr);
         ImGui::Image(srv, imSize);
         ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

         ImGui::End();
      }
   }

   void ViewportWindow::OnUpdate(float dt) {
      // todo:
      if (!enableInput) {
         return;
      }

      camera.NextFrame(); // todo:

      if (Input::IsKeyPressed(VK_LBUTTON) && !ImGuizmo::IsOver()) {
         selectEntityUnderCursor = true;
      }

      // todo:
      // ImGui::SetWindowFocus(name.data());
      camera.Update(dt);

      if (!Input::IsKeyPressed(VK_RBUTTON)) {
         if (Input::IsKeyPressed('W')) {
            gizmoCfg.operation = ImGuizmo::OPERATION::TRANSLATE;
         }
         if (Input::IsKeyPressed('R')) {
            gizmoCfg.operation = ImGuizmo::OPERATION::ROTATE;
         }
         if (Input::IsKeyPressed('S')) {
            gizmoCfg.operation = ImGuizmo::OPERATION::SCALE;
         }

         // todo: IsKeyDown
         // if (Input::IsKeyPressed('Q')) {
         //    gizmoCfg.space = 1 - gizmoCfg.space;
         // }
         if (Input::IsKeyPressed('Q')) {
            gizmoCfg.space = 0;
         }
         if (Input::IsKeyPressed('A')) {
            gizmoCfg.space = 1;
         }

         if (Input::IsKeyPressed('F') && selection->FirstSelected()) {
            auto selectedEntity = selection->FirstSelected();
            camera.position = selectedEntity.Get<SceneTransformComponent>().position - camera.Forward() * 3.f;
         }
      }

      // todo:
      static float acc = 0;
      acc += dt;
      if (acc > 0.2 && Input::IsKeyPressed(' ')) {
         Entity shootRoot = scene->FindByName("Shoots");
         if (!shootRoot) {
            shootRoot = scene->Create("Shoots");
         }

         acc = 0;
         auto shoot = scene->Create(shootRoot, "Shoot cube");
         shoot.Get<SceneTransformComponent>().SetPosition(camera.position);
         shoot.Add<MaterialComponent>();
         shoot.Add<GeometryComponent>();

         // todo:
         RigidBodyComponent _rb;
         _rb.dynamic = true;

         auto& rb = shoot.Add<RigidBodyComponent>(_rb);
         rb.SetLinearVelocity(camera.Forward() * 50.f);
      }

      // INFO("Left {} Right {}", Input::IsKeyPressed(VK_LBUTTON), Input::IsKeyPressed(VK_RBUTTON));
   }

   void ViewportWindow::Gizmo(const vec2& contentRegion, const vec2& cursorPos) {
      auto selectedEntity = selection->FirstSelected();
      if (!selectedEntity.Valid()) {
         return;
      }

      ImGuizmo::SetOrthographic(false);
      ImGuizmo::SetDrawlist();
      ImGuizmo::SetRect(cursorPos.x, cursorPos.y, contentRegion.x, contentRegion.y);

      bool snap = Input::IsKeyPressed(VK_CONTROL);

      auto& trans = selectedEntity.Get<SceneTransformComponent>();
      mat4 entityTransform = trans.GetMatrix();

      float snapValue = 1;
      float snapValues[3] = { snapValue, snapValue, snapValue };

      ImGuizmo::Manipulate(glm::value_ptr(camera.view),
         glm::value_ptr(camera.projection),
         (ImGuizmo::OPERATION)gizmoCfg.operation,
         (ImGuizmo::MODE)gizmoCfg.space,
         glm::value_ptr(entityTransform),
         nullptr,
         snap ? snapValues : nullptr);

      if (ImGuizmo::IsUsing()) {
         Undo::Get().SaveToStack(selectedEntity, true);

         auto [position, rotation, scale] = GetTransformDecomposition(entityTransform);
         if (gizmoCfg.operation & ImGuizmo::OPERATION::TRANSLATE) {
            trans.SetPosition(position);
         }
         if (gizmoCfg.operation & ImGuizmo::OPERATION::ROTATE) {
            trans.SetRotation(rotation);
         }
         if (gizmoCfg.operation & ImGuizmo::OPERATION::SCALE) {
            trans.SetScale(scale);
         }
      }
   }
}