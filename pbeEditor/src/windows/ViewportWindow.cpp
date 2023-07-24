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
#include "gui/Gui.h"
#include "scene/Utils.h"
#include "utils/TimedAction.h"


namespace pbe {

   // todo:
   void ImGui_SetPointSampler(const ImDrawList* list, const ImDrawCmd* cmd) {
      sDevice->g_pd3dDeviceContext->PSSetSamplers(0, 1, &rendres::samplerStateWrapPoint);
   }

   class TextureViewWindow : public EditorWindow {
   public:
      TextureViewWindow() : EditorWindow("Texture View") {}

      void OnImGuiRender() override {
         UI_WINDOW("Texture View"); // todo:
         if (!texture) {
            ImGui::Text("No texture selected");
            return;
         }

         const auto& desc = texture->GetDesc();
         ImGui::Text("%s (%dx%d, %d)", desc.name.c_str(), desc.size.x, desc.size.y, desc.mips);

         static int iMip = 0;
         ImGui::SliderInt("mip", &iMip, 0, desc.mips - 1);
         ImGui::SameLine();

         int d = 1 << iMip;
         ImGui::Text("mip size (%dx%d)", desc.size.x / d, desc.size.y / d);

         auto imSize = ImGui::GetContentRegionAvail();
         auto srv = texture->GetMipSrv(iMip);

         ImGui::GetWindowDrawList()->AddCallback(ImGui_SetPointSampler, nullptr);
         ImGui::Image(srv, imSize);
         ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
      }

      Texture2D* texture{};
   } gTextureViewWindow;

   // todo: add hotkey remove cfg
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
      enableInput = ImGui::IsWindowHovered();

      if (customHeadFunc) {
         customHeadFunc();
         // todo:
         ImGui::SameLine();
      }

      static int item_current = 0;
      const char* items[] = { "ColorLDR", "ColorHDR", "Normal", "SSAO"};

      ImGui::SetNextItemWidth(80);
      ImGui::Combo("Scene RTs", &item_current, items, IM_ARRAYSIZE(items));
      ImGui::SameLine();

      static float renderScale = 1;
      ImGui::SetNextItemWidth(100);
      ImGui::SliderFloat("Scale", &renderScale, 0.1f, 2.f);
      ImGui::SameLine();

      // todo:
      static bool textureViewWindow = false;
      ImGui::Checkbox("Texture View", &textureViewWindow);

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

      auto imSize = ImGui::GetContentRegionAvail();
      int2 size = { imSize.x, imSize.y };
      if (renderScale != 1.f) {
         size = vec2(size) * renderScale;
      }

      if (all(size > 1)) {
         if (!renderContext.colorHDR || renderContext.colorHDR->GetDesc().size != size) {
            renderContext = CreateRenderContext(size);
            camera.UpdateProj(size);
            camera.NextFrame();
         }

         vec2 mousePos = { ImGui::GetMousePos().x, ImGui::GetMousePos().y };
         vec2 cursorPos = { ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y };

         int2 cursorPixelIdx{ mousePos - cursorPos};

         cmd.SetCommonSamplers();
         if (scene) {
            renderContext.cursorPixelIdx = cursorPixelIdx;

            Entity e = scene->GetAnyWithComponent<CameraComponent>();
            if (!freeCamera && e) {
               auto& trans = e.Get<SceneTransformComponent>();
               camera.position = trans.Position();
               camera.SetViewDirection(trans.Forward());
            }

            renderer->RenderScene(cmd, *scene, camera, renderContext);
         }
         cmd.pContext->ClearState(); // todo:

         Texture2D* sceneRTs[] = { renderContext.colorLDR, renderContext.colorHDR,renderContext.normalTex, renderContext.ssao};

         Texture2D* image = sceneRTs[item_current];
         auto srv = image->srv.Get();
         ImGui::Image(srv, imSize);

         if (zoomEnable) {
            Zoom(*image, cursorPixelIdx);
         }

         Gizmo(vec2{ imSize.x, imSize.y }, cursorPos);

         if (textureViewWindow) {
            gTextureViewWindow.texture = renderContext.linearDepth;
            gTextureViewWindow.OnImGuiRender();
         }
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

      if (Input::IsKeyDown('X')) { // todo: other key
         freeCamera = !freeCamera;
      }

      // todo:
      // ImGui::SetWindowFocus(name.data());
      camera.Update(dt);

      if (Input::IsKeyDown(VK_RBUTTON)) {
         Input::HideMouse(true);
      }
      if (Input::IsKeyUp(VK_RBUTTON)) {
         // todo: handle window lost focus
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

      // zoom
      if (Input::IsKeyDown('V')) {
         zoomEnable = !zoomEnable;
      }

      // todo:
      static TimedAction timer{5};
      if (timer.Update(dt) > 0 && Input::IsKeyPressing(' ')) {
         Entity shootRoot = scene->FindByName("Shoots");
         if (!shootRoot) {
            shootRoot = scene->Create("Shoots");
         }

         auto shoot = CreateCube(*scene, CubeDesc{
               .parent = shootRoot,
               .namePrefix = "Shoot cube",
               .pos = camera.position,
            });

         auto& rb = shoot.Get<RigidBodyComponent>();
         rb.SetLinearVelocity(camera.Forward() * 50.f);
      }
   }

   void ViewportWindow::Zoom(Texture2D& image, vec2 center) {
      vec2 zoomImageSize{ 300, 300 };

      vec2 uvCenter = center / vec2{ image.GetDesc().size };

      if (all(uvCenter > vec2{ 0 } && uvCenter < vec2{ 1 })) {
         vec2 uvScale{ zoomScale / 2.f };

         vec2 uv0 = uvCenter - uvScale;
         vec2 uv1 = uvCenter + uvScale;

         ImGui::BeginTooltip();

         ImGui::GetWindowDrawList()->AddCallback(ImGui_SetPointSampler, nullptr);
         ImGui::Image(image.srv.Get(), { zoomImageSize.x, zoomImageSize.y }, { uv0.x, uv0.y }, { uv1.x, uv1.y });
         ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

         ImGui::EndTooltip();
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
