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
#include "typer/Registration.h"
#include "app/Window.h"
#include "gui/Gui.h"
#include "physics/PhysComponents.h"
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

      void OnWindowUI() override {
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

   ViewportWindow::ViewportWindow(std::string_view name) : EditorWindow(name) {
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

   void ViewportWindow::OnBefore() {
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
   }

   void ViewportWindow::OnAfter() {
      ImGui::PopStyleVar();
   }

   void ViewportWindow::OnWindowUI() {
      static int item_current = 0;
      static float renderScale = 1;
      // todo:
      static bool textureViewWindow = false;

      auto viewportCursorPos = ImGui::GetCursorScreenPos();
      auto startCursorPos = ImGui::GetCursorPos();

      static bool viewportToolsWindow = true;
      if (viewportToolsWindow) {
         UI_PUSH_STYLE_COLOR(ImGuiCol_WindowBg, (ImVec4{ 0, 0, 0, 0.5 }));

         UI_PUSH_STYLE_VAR(ImGuiStyleVar_WindowRounding, 10);
         UI_PUSH_STYLE_VAR(ImGuiStyleVar_WindowPadding, (ImVec2{ 5, 5 }));

         if (UI_WINDOW("Viewport tools", &viewportToolsWindow,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
            ImGui::SetWindowPos(viewportCursorPos + ImVec2{ 20, 5 });

            UI_PUSH_STYLE_VAR(ImGuiStyleVar_FrameBorderSize, 1);
            UI_PUSH_STYLE_VAR(ImGuiStyleVar_FrameRounding, 10);

            const char* items[] = { "ColorLDR", "ColorHDR", "Normal", "SSAO" };

            ImGui::SetNextItemWidth(90);
            ImGui::Combo("##Scene RTs", &item_current, items, IM_ARRAYSIZE(items));
            ImGui::SameLine(0, 0);

            ImGui::SetNextItemWidth(100);
            ImGui::SliderFloat("Scale", &renderScale, 0.1f, 2.f);
            ImGui::SameLine();

            // todo:
            ImGui::Checkbox("Texture View", &textureViewWindow);
            ImGui::SameLine();

            ImGui::SetWindowSize(ImVec2{ -1, -1 });
         }
      }

      CommandList cmd{ sDevice->g_pd3dDeviceContext };

      if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && Input::IsKeyDown(VK_LBUTTON) && !ImGuizmo::IsOver()) {
         auto entityID = renderer->GetEntityIDUnderCursor(cmd);

         bool clearPrevSelection = !Input::IsKeyPressing(VK_CONTROL);
         if (entityID != (uint)-1) {
            Entity e{ entt::entity(entityID), scene };
            selection->ToggleSelect(e, clearPrevSelection);
         } else if (clearPrevSelection) {
            selection->ClearSelection();
         }
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

         // todo: window style
         if (Input::IsKeyPressing(VK_SHIFT) && Input::IsKeyDown('A')) {
            ImGui::OpenPopup("Add");
         }

         if (UI_POPUP("Add")) {
            ImGui::Text("Add");
            ImGui::Separator();

            Entity addedEntity = SceneAddEntityMenu(*scene);
            if (addedEntity) {
               selection->ToggleSelect(addedEntity);
               // ImGui::CloseCurrentPopup();
            }
         }

         if (zoomEnable) {
            Zoom(*image, cursorPixelIdx);
         }

         Gizmo(vec2{ imSize.x, imSize.y }, cursorPos);

         if (textureViewWindow) {
            gTextureViewWindow.texture = renderContext.linearDepth;
            gTextureViewWindow.OnWindowUI();
         }
      }

      ImGui::SetCursorPos(startCursorPos + ImVec2{ 3, 5 });

      if (ImGui::Button("=")) {
         viewportToolsWindow = !viewportToolsWindow;
      }
   }

   void ViewportWindow::OnUpdate(float dt) {
      // todo:
      if (!focused) {
         return;
      }

      camera.NextFrame(); // todo:

      if (Input::IsKeyDown(VK_RBUTTON)) {
         cameraMove = true;
         Input::HideMouse(true);
      }
      if (cameraMove && Input::IsKeyUp(VK_RBUTTON)) {
         cameraMove = false;
         Input::ShowMouse(true);
      }

      if (cameraMove) {
         camera.Update(dt);
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

      if (Input::IsKeyDown('X')) { // todo: other key
         freeCamera = !freeCamera;
      }

      if (Input::IsKeyDown('C')) { // todo: other key
         if (Entity e = selection->FirstSelected()) {
            e.Get<SceneTransformComponent>().SetPosition(camera.position);
            // todo: set rotation
         }
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

   void ViewportWindow::OnLostFocus() {
      INFO("ViewportWindow::OnLostFocus");
      if (cameraMove) {
         cameraMove = false;
         Input::ShowMouse(true);
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
