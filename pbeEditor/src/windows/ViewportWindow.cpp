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
#include "app/Window.h"


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
   }

   ViewportWindow::~ViewportWindow() {
      Serialize(viewportSettingPath,
         ViewportSettings{.cameraPos = camera.position, .cameraAngles = camera.cameraAngle});
   }

   void ViewportWindow::OnImGuiRender() {
      ImGui::Begin(name.c_str(), &show);

      enableInput = ImGui::IsWindowHovered();

      if (customHeadFunc) {
         customHeadFunc();
      }

      static bool textureViewWindow = false;
      ImGui::Checkbox("Texture View Window", &textureViewWindow);
      ImGui::SameLine();

      static float renderScale = 1;
      ImGui::SetNextItemWidth(120);
      ImGui::SliderFloat("Scale", &renderScale, 0.1f, 2.f);
      ImGui::SameLine();

      static int item_current = 0;
      const char* items[] = { "ColorLDR", "ColorHDR", "Normal", "SSAO"};

      ImGui::SetNextItemWidth(80);
      ImGui::Combo("Scene RTs", &item_current, items, IM_ARRAYSIZE(items));

      CommandList cmd{ sDevice->g_pd3dDeviceContext };

      if (selectEntityUnderCursor) {
         auto entityID = renderer->GetEntityIDUnderCursor(cmd);

         bool clearPrevSelection = !Input::IsKeyPressing(VK_CONTROL);
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
      if (renderScale != 1.f) {
         size = vec2(size) * renderScale;
      }

      if (size.x > 1 && size.y > 1) {
         if (!renderContext.colorHDR || renderContext.colorHDR->GetDesc().size != size) {
            renderContext = CreateRenderContext(size);
            camera.UpdateProj(size);
            camera.NextFrame();
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
            renderContext.cursorPixelIdx = cursorPixelIdx;
            renderer->RenderScene(cmd, *scene, rendCamera, renderContext);
         }
         cmd.pContext->ClearState(); // todo:

         Texture2D* sceneRTs[] = { renderContext.colorLDR, renderContext.colorHDR,renderContext.normalTex, renderContext.ssao};
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

      if (textureViewWindow && renderContext.linearDepth) {
         ImGui::Begin("Texture View");

         auto texture = renderContext.linearDepth;

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

      camera.NextFrame();

      if (Input::IsKeyDown(VK_LBUTTON) && !ImGuizmo::IsOver()) {
         selectEntityUnderCursor = true;
      }

      // todo:
      // ImGui::SetWindowFocus(name.data());
      camera.Update(dt);

      if (Input::IsKeyDown(VK_RBUTTON)) {
         Input::HideMouse(true);
      }
      if (Input::IsKeyUp(VK_RBUTTON)) {
         Input::ShowMouse(true);
      }

      if (!Input::IsKeyPressing(VK_RBUTTON)) {
         if (Input::IsKeyDown('W')) {
            gizmoCfg.operation = ImGuizmo::OPERATION::TRANSLATE;
         }
         if (Input::IsKeyDown('R')) {
            gizmoCfg.operation = ImGuizmo::OPERATION::ROTATE;
         }
         if (Input::IsKeyDown('S')) {
            gizmoCfg.operation = ImGuizmo::OPERATION::SCALE;
         }

         if (Input::IsKeyDown('Q')) {
            gizmoCfg.space = 1 - gizmoCfg.space;
         }

         if (Input::IsKeyDown('F') && selection->FirstSelected()) {
            auto selectedEntity = selection->FirstSelected();
            camera.position = selectedEntity.GetTransform().Position() - camera.Forward() * 3.f;
         }
      }

      // todo:
      static float acc = 0;
      acc += dt;
      if (acc > 0.2 && Input::IsKeyPressing(' ')) {
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
   }

   void ViewportWindow::Gizmo(const vec2& contentRegion, const vec2& cursorPos) {
      auto selectedEntity = selection->FirstSelected();
      if (!selectedEntity.Valid()) {
         return;
      }

      ImGuizmo::SetOrthographic(false);
      ImGuizmo::SetDrawlist();
      ImGuizmo::SetRect(cursorPos.x, cursorPos.y, contentRegion.x, contentRegion.y);

      bool snap = Input::IsKeyPressing(VK_CONTROL);

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
