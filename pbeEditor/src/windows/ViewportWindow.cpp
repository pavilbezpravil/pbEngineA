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
#include "physics/PhysicsScene.h"
#include "scene/Utils.h"
#include "utils/TimedAction.h"
#include "physics/PhysQuery.h"
#include "rend/DbgRend.h"


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
   };

   // todo: add hotkey remove cfg
   CVarSlider<float> zoomScale{ "zoom/scale", 0.05f, 0.f, 1.f };
   CVarValue<bool> cvGizmo{ "gizmo", false };

   enum class EditorShowTexture : int {
      Lit,
      Unlit,
      Normal,
      Roughness,
      Metallic,
      Diffuse,
      Specular,
      Motion,
   };

   constexpr char viewportSettingPath[] = "scene_viewport.yaml";

   STRUCT_BEGIN(ViewportSettings)
      STRUCT_FIELD(cameraPos)
      STRUCT_FIELD(cameraAngles)
      STRUCT_FIELD(showToolbar)
      STRUCT_FIELD(renderScale)
      STRUCT_FIELD(showedTexIdx)
      STRUCT_FIELD(rayTracingRendering)
   STRUCT_END()

   ViewportWindow::ViewportWindow(std::string_view name) : EditorWindow(name) {
      Deserialize(viewportSettingPath, settings);

      camera.position = settings.cameraPos;
      camera.cameraAngle = settings.cameraAngles;
      camera.UpdateView();
   }

   ViewportWindow::~ViewportWindow() {
      settings.cameraPos = camera.position;
      settings.cameraAngles = camera.cameraAngle;

      Serialize(viewportSettingPath, settings);
   }

   void ViewportWindow::OnBefore() {
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
   }

   void ViewportWindow::OnAfter() {
      ImGui::PopStyleVar();
   }

   void ViewportWindow::OnWindowUI() {
      auto contentRegion = ImGui::GetContentRegionAvail();
      int2 size = vec2{ contentRegion.x, contentRegion.y } * settings.renderScale;

      if (!all(size > 1)) {
         return;
      }

      if (!renderContext.colorHDR || renderContext.colorHDR->GetDesc().size != size) {
         renderContext = CreateRenderContext(size);
         camera.UpdateProj(size);
         camera.NextFrame();
      }

      vec2 mousePos = ToVec2(ImGui::GetMousePos());
      vec2 cursorPos = ToVec2(ImGui::GetCursorScreenPos());

      int2 cursorPixelIdx{ (mousePos - cursorPos) * settings.renderScale };
      vec2 cursorUV = vec2(cursorPixelIdx) / vec2(size);

      CommandList cmd{ sDevice->g_pd3dDeviceContext };

      if (state == ViewportState::None && Input::IsKeyDown(KeyCode::LeftButton) && !ImGuizmo::IsOver()) {
         if (Input::IsKeyPressing(KeyCode::Ctrl)) {
            ApplyDamageFromCamera(camera.GetWorldSpaceRayDirFromUV(cursorUV));
         } else {
            SelectEntity((EntityID)renderer->GetEntityIDUnderCursor(cmd));
         }
      }

      cmd.SetCommonSamplers();
      if (scene) {
         renderContext.cursorPixelIdx = cursorPixelIdx;

         Entity cameraEntity = scene->GetAnyWithComponent<CameraComponent>();
         if (!freeCamera && cameraEntity) {
            auto& trans = cameraEntity.Get<SceneTransformComponent>();
            camera.position = trans.Position();
            camera.SetViewDirection(trans.Forward());
         }

         // todo:
         DbgRend& dbgRend = *scene->dbgRend;
         for (auto selectedEntity : selection->selected) {
            auto pos = selectedEntity.GetTransform().Position();
            dbgRend.DrawSphere(Sphere{ pos, 0.07f }, Color_Yellow, false);
         }

         renderer->RenderScene(cmd, *scene, camera, renderContext);
      }

      Texture2D& image = *renderContext.colorLDR;

      DrawTexture(cmd, image, contentRegion);

      AddEntityPopup(cursorUV);

      Zoom(image, cursorPixelIdx);

      if (cvGizmo) {
         Gizmo(vec2{ contentRegion.x, contentRegion.y }, cursorPos);
      } else {
         Manipulator(cursorUV);
      }

      ImGui::SetCursorPos(ImVec2{ 3, 5 });
      if (ImGui::Button("=")) {
         settings.showToolbar = !settings.showToolbar;
      }

      ViewportToolbar();
   }

   void ViewportWindow::OnUpdate(float dt) {
      // todo:
      if (!focused) {
         return;
      }

      if (Input::IsKeyPressing(KeyCode::RightButton)) {
         StartCameraMove();
      }
      if (Input::IsKeyUp(KeyCode::RightButton)) {
         StopCameraMove();
      }

      camera.NextFrame(); // todo:

      if (state == ViewportState::CameraMove) {
         camera.Update(dt);
      }

      if (state == ViewportState::None) {
         if (Input::IsKeyPressing(KeyCode::Shift)) {
            if (Input::IsKeyDown(KeyCode::D)) {
               auto prevSelected = selection->selected;
               selection->ClearSelection();

               for (auto entity : prevSelected) {
                  auto duplicatedEntity = scene->Duplicate(entity);
                  selection->Select(duplicatedEntity, false);
               }

               state = ViewportState::ObjManipulation;
               manipulatorMode = Translate | AllAxis;
            }
         }

         // select parent
         if (Input::IsKeyDown(KeyCode::Q)) {
            if (selection->LastSelected()) {
               auto parent = selection->LastSelected().GetTransform().parent;
               if (parent.GetTransform().parent) { // not root
                  selection->Select(parent, !Input::IsKeyPressing(KeyCode::Shift));
               }
            }
         }

         if (Input::IsKeyDown(KeyCode::F) && selection->LastSelected()) {
            auto selectedEntity = selection->LastSelected();
            camera.position = selectedEntity.GetTransform().Position() - camera.Forward() * 3.f;
            camera.UpdateView();
         }

         // todo: remove
         if (Input::IsKeyDown(KeyCode::W)) {
            gizmoCfg.operation = ImGuizmo::OPERATION::TRANSLATE;
         }
         if (Input::IsKeyDown(KeyCode::R)) {
            gizmoCfg.operation = ImGuizmo::OPERATION::ROTATE;
         }
         if (Input::IsKeyDown(KeyCode::S)) {
            gizmoCfg.operation = ImGuizmo::OPERATION::SCALE;
         }
         if (Input::IsKeyDown(KeyCode::Q)) {
            gizmoCfg.space = 1 - gizmoCfg.space;
         }

         if (Input::IsKeyDown(KeyCode::X)) { // todo: other key
            freeCamera = !freeCamera;
         }

         if (Input::IsKeyDown(KeyCode::C)) { // todo: other key
            if (Entity e = selection->LastSelected()) {
               e.Get<SceneTransformComponent>().SetPosition(camera.position);
               // todo: set rotation
            }
         }

         // todo:
         static TimedAction timer{ 5 };
         bool doShoot = Input::IsKeyPressing(KeyCode::Space);
         if (timer.Update(dt, doShoot ? -1 : 1) > 0 && doShoot) {
            Entity shootRoot = scene->FindByName("Shoots");
            if (!shootRoot) {
               shootRoot = scene->Create("Shoots");
            }

            auto shoot = CreateCube(*scene, CubeDesc{
                  .parent = shootRoot,
                  .namePrefix = "Shoot cube",
                  .pos = camera.position,
                  .scale = vec3_One,
               });

            auto& rb = shoot.Get<RigidBodyComponent>();
            rb.SetLinearVelocity(camera.Forward() * 25.f);
            rb.MakeDestructable();
            rb.hardness = 10.f;

            shoot.Add<TimedDieComponent>().SetRandomDieTime(5, 10);
         }
      }
   }

   void ViewportWindow::OnLostFocus() {
      StopCameraMove();
      StopManipulator();
   }

   void ViewportWindow::Zoom(Texture2D& image, vec2 center) {
      if (Input::IsKeyDown(KeyCode::V)) {
         zoomEnable = !zoomEnable;
      }

      if (!zoomEnable) {
         return;
      }

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
      auto selectedEntity = selection->LastSelected();
      if (!selectedEntity.Valid()) {
         return;
      }

      ImGuizmo::SetOrthographic(false);
      ImGuizmo::SetDrawlist();
      ImGuizmo::SetRect(cursorPos.x, cursorPos.y, contentRegion.x, contentRegion.y);

      bool snap = Input::IsKeyPressing(KeyCode::Ctrl);

      auto& trans = selectedEntity.Get<SceneTransformComponent>();
      mat4 entityTransform = trans.GetWorldMatrix();

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

         selectedEntity.AddOrReplace<TransformChangedMarker>();
      }
   }

   void ViewportWindow::ViewportToolbar() {
      if (!settings.showToolbar) {
         return;
      }

      UI_PUSH_STYLE_COLOR(ImGuiCol_ChildBg, (ImVec4{ 0, 0, 0, 0.2f }));
      UI_PUSH_STYLE_VAR(ImGuiStyleVar_ChildRounding, 10);
      // UI_PUSH_STYLE_VAR(ImGuiStyleVar_WindowPadding, (ImVec2{ 5, 5 }));

      ImGui::SetCursorPos(ImVec2{ 20, 10 });
      if (UI_CHILD_WINDOW("Viewport tools", (ImVec2{ 300, ImGui::GetFrameHeight() }))) {
         UI_PUSH_STYLE_VAR(ImGuiStyleVar_FrameBorderSize, 1);
         UI_PUSH_STYLE_VAR(ImGuiStyleVar_FrameRounding, 10);

         const char* items[] = { "Lit", "Unlit", "Normal", "Roughness", "Metallic", "Diffuse", "Specular",  "Motion" };

         ImGui::SetNextItemWidth(90);
         ImGui::Combo("##Scene RTs", &settings.showedTexIdx, items, IM_ARRAYSIZE(items));
         ImGui::SameLine(0, 0);

         ImGui::SetNextItemWidth(100);
         ImGui::SliderFloat("Scale", &settings.renderScale, 0.1f, 2.f);
         ImGui::SameLine();

         ImGui::Checkbox("RT", &settings.rayTracingRendering);
         rayTracingSceneRender = settings.rayTracingRendering;
         ImGui::SameLine();

         // todo:
         // if (UI_MENU("Visualize")) {
         //    if (ImGui::MenuItem("Scene")) {
         //
         //    }
         //    if (ImGui::MenuItem("Physics")) {
         //
         //    }
         // }
      }
   }

   void ViewportWindow::ApplyDamageFromCamera(const vec3& rayDirection) {
      if (auto hitResult = scene->GetPhysics()->RayCast(camera.position, rayDirection, 10000.f)) {
         if (auto rb = hitResult.physActor.TryGet<RigidBodyComponent>()) {
            rb->ApplyDamage(hitResult.position, 1000.f);
         }
      }
   }

   void ViewportWindow::SelectEntity(EntityID entityID) {
      bool clearPrevSelection = !Input::IsKeyPressing(KeyCode::Shift);
      if (entityID != NullEntityID) {
         Entity entity{ entityID, scene };
         selection->ToggleSelect(entity, clearPrevSelection);
      } else if (clearPrevSelection) {
         selection->ClearSelection();
      }
   }

   void ViewportWindow::DrawTexture(CommandList& cmd, Texture2D& image, const ImVec2& imSize) {
      if ((EditorShowTexture)settings.showedTexIdx != EditorShowTexture::Lit) {
         GPU_MARKER("EditorTexShow");

         cmd.SetRenderTargets();

         auto passDesc = ProgramDesc::Cs("editorTexShow.hlsl", "main");

         Texture2D* texIn = nullptr;
         switch ((EditorShowTexture)settings.showedTexIdx) {
         case EditorShowTexture::Lit:
            ASSERT(false);
            break;
         case EditorShowTexture::Unlit:
            texIn = renderContext.baseColorTex;
            passDesc.cs.defines.AddDefine("UNLIT");
            break;
         case EditorShowTexture::Normal:
            texIn = renderContext.normalTex;
            passDesc.cs.defines.AddDefine("NORMAL");
            break;
         case EditorShowTexture::Roughness:
            texIn = renderContext.normalTex;
            passDesc.cs.defines.AddDefine("ROUGHNESS");
            break;
         case EditorShowTexture::Metallic:
            texIn = renderContext.baseColorTex;
            passDesc.cs.defines.AddDefine("METALLIC");
            break;
         case EditorShowTexture::Diffuse:
            texIn = renderContext.diffuseHistoryTex; // todo: without denoise must be unfiltered
            passDesc.cs.defines.AddDefine("DIFFUSE_SPECULAR");
            break;
         case EditorShowTexture::Specular:
            texIn = renderContext.specularHistoryTex; // todo: without denoise must be unfiltered
            passDesc.cs.defines.AddDefine("DIFFUSE_SPECULAR");
            break;
         case EditorShowTexture::Motion:
            texIn = renderContext.motionTex;
            passDesc.cs.defines.AddDefine("MOTION");
            break;
         default:
            break;
         }

         auto editorTexShowPass = GetGpuProgram(passDesc);

         editorTexShowPass->Activate(cmd);

         editorTexShowPass->SetSRV(cmd, "gIn", texIn);
         editorTexShowPass->SetUAV(cmd, "gOut", image);

         cmd.Dispatch2D(image.GetDesc().size, int2{ 8 });
      }

      cmd.pContext->ClearState(); // todo:

      auto srv = image.srv.Get();
      ImGui::Image(srv, imSize);
   }

   void ViewportWindow::AddEntityPopup(const vec2& cursorUV) {
      static vec2 spawnCursorUV;
      if (state == ViewportState::None && Input::IsKeyPressing(KeyCode::Shift) && Input::IsKeyDown(KeyCode::A)) {
         ImGui::OpenPopup("Add");
         spawnCursorUV = cursorUV;
      }

      UI_PUSH_STYLE_VAR(ImGuiStyleVar_WindowPadding, (ImVec2{ 7, 7 }));
      UI_PUSH_STYLE_VAR(ImGuiStyleVar_PopupRounding, 7);

      const auto& colorBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
      UI_PUSH_STYLE_COLOR(ImGuiCol_PopupBg, (ImVec4{ colorBg.x, colorBg.y, colorBg.z, 0.75 }));

      if (UI_POPUP("Add")) {
         ImGui::Text("Add");
         ImGui::Separator();

         auto spawnPosHint = std::invoke([&] {
            vec3 dir = camera.GetWorldSpaceRayDirFromUV(spawnCursorUV);
            if (auto hit = scene->GetPhysics()->Sweep(camera.position, dir, 100.f)) {
               return camera.position + dir * hit.distance;
            } else {
               return camera.position + camera.Forward() * 3.f;
            }
         });

         Entity addedEntity = SceneAddEntityMenu(*scene, spawnPosHint, selection);
         if (addedEntity) {
            selection->ToggleSelect(addedEntity);
         }
      }
   }

   void ViewportWindow::Manipulator(const vec2& cursorUV) {
      if (!selection->HasSelection() || state == ViewportState::CameraMove) {
         return;
      }

      auto& dbgRend = *scene->dbgRend;

      Entity entity = selection->LastSelected();
      auto relativePos = manipulatorRelativeTransform.position;

      vec3 cameraDir = camera.GetWorldSpaceRayDirFromUV(cursorUV);

      Plane plane = Plane::FromPointNormal(relativePos, camera.Forward());
      Ray ray = Ray{ camera.position, cameraDir };

      auto currentPlanePos = plane.RayIntersectionAt(ray);

      if (state == ViewportState::None) {
         manipulatorRelativeTransform = {
            entity.GetTransform().Position(),
            entity.GetTransform().Rotation(),
            entity.GetTransform().Scale(),
         };

         manipulatorInitialPos = currentPlanePos;
      }

      if (Input::IsKeyDown(KeyCode::G)) {
         state = ViewportState::ObjManipulation;
         manipulatorMode = Translate | AllAxis;
      }
      if (Input::IsKeyDown(KeyCode::R)) {
         state = ViewportState::ObjManipulation;
         manipulatorMode = Rotate | AllAxis;
      }
      if (Input::IsKeyDown(KeyCode::S)) {
         state = ViewportState::ObjManipulation;
         manipulatorMode = Scale | AllAxis;
      }

      if (state != ViewportState::ObjManipulation) {
         return;
      }

      ManipulatorResetTransforms();

      if (Input::IsKeyDown(KeyCode::RightButton)) {
         state = ViewportState::None;
         return;
      }

      if (Input::IsKeyDown(KeyCode::X)) {
         manipulatorMode &= ~AllAxis;
         manipulatorMode |= AxisX;
      }
      if (Input::IsKeyDown(KeyCode::Y)) {
         manipulatorMode &= ~AllAxis;
         manipulatorMode |= AxisY;
      }
      if (Input::IsKeyDown(KeyCode::Z)) {
         manipulatorMode &= ~AllAxis;
         manipulatorMode |= AxisZ;
      }

      dbgRend.DrawSphere(Sphere{ relativePos, 0.03f }, Color_Yellow, false);

      if (manipulatorMode & (Rotate | Scale)) {
         dbgRend.DrawLine(relativePos, currentPlanePos, Color_Black, false);
      }

      if ((manipulatorMode & AllAxis) != AllAxis) {
         const float AXIS_LENGTH = 100000.f;
         if (manipulatorMode & AxisX) {
            dbgRend.DrawLine(relativePos - vec3_X * AXIS_LENGTH, relativePos + vec3_X * AXIS_LENGTH, Color_Red, false);
         } else if (manipulatorMode & AxisY) {
            dbgRend.DrawLine(relativePos - vec3_Y * AXIS_LENGTH, relativePos + vec3_Y * AXIS_LENGTH, Color_Green, false);
         } else if (manipulatorMode & AxisZ) {
            dbgRend.DrawLine(relativePos - vec3_Z * AXIS_LENGTH, relativePos + vec3_Z * AXIS_LENGTH, Color_Blue, false);
         }
      }

      if (manipulatorMode & Translate) {
         vec3 translation = currentPlanePos - manipulatorInitialPos;

         vec3 translationProcessed = vec3{ 0 };

         if (manipulatorMode & AxisX) {
            translationProcessed.x = dot(translation, vec3_X);
         }
         if (manipulatorMode & AxisY) {
            translationProcessed.y = dot(translation, vec3_Y);
         }
         if (manipulatorMode & AxisZ) {
            translationProcessed.z = dot(translation, vec3_Z);
         }

         entity.GetTransform().SetPosition(manipulatorRelativeTransform.position + translationProcessed);
      }

      if (manipulatorMode & Rotate) {
         vec3 initialDir = normalize(manipulatorInitialPos - relativePos);
         vec3 currentDir = normalize(currentPlanePos - relativePos);

         vec3 cross = glm::cross(initialDir, currentDir);
         float dot = glm::dot(initialDir, currentDir);

         float angle = acosf(dot) * glm::sign(glm::dot(camera.Forward(), cross));

         vec3 axis;
         if ((manipulatorMode & AllAxis) == AllAxis) {
            axis = camera.Forward();
         } else {
            if (manipulatorMode & AxisX) {
               axis = vec3_X;
            } else if (manipulatorMode & AxisY) {
               axis = vec3_Y;
            } else if (manipulatorMode & AxisZ) {
               axis = vec3_Z;
            }

            angle *= glm::sign(glm::dot(camera.Forward(), axis));
         }

         quat rotation = glm::angleAxis(angle, axis);
         entity.GetTransform().SetRotation(rotation * manipulatorRelativeTransform.rotation);
      }

      if (manipulatorMode & Scale) {
         float initialDistance = glm::distance(manipulatorInitialPos, relativePos);
         float currentDistance = glm::distance(currentPlanePos, relativePos);
         float scale = currentDistance / initialDistance;

         vec3 scale3 = vec3{
            manipulatorMode & AxisX ? scale : 1.f,
            manipulatorMode & AxisY ? scale : 1.f,
            manipulatorMode & AxisZ ? scale : 1.f,
         };

         entity.GetTransform().SetScale(manipulatorRelativeTransform.scale * scale3);
      }

      entity.AddOrReplace<TransformChangedMarker>();

      if (Input::IsKeyDown(KeyCode::LeftButton)) {
         state = ViewportState::None;
      }
   }

   void ViewportWindow::ManipulatorResetTransforms() {
      if (state != ViewportState::ObjManipulation || !selection->HasSelection()) {
         return;
      }

      Entity entity = selection->LastSelected();

      entity.GetTransform().SetPosition(manipulatorRelativeTransform.position);
      entity.GetTransform().SetRotation(manipulatorRelativeTransform.rotation);
      entity.GetTransform().SetScale(manipulatorRelativeTransform.scale);
   }

   void ViewportWindow::StopManipulator() {
      if (state == ViewportState::ObjManipulation) {
         ManipulatorResetTransforms();

         state = ViewportState::None;
      }
   }

   void ViewportWindow::StartCameraMove() {
      if (state == ViewportState::None) {
         state = ViewportState::CameraMove;
         Input::HideMouse(true);
      }
   }

   void ViewportWindow::StopCameraMove() {
      if (state == ViewportState::CameraMove) {
         state = ViewportState::None;
         Input::ShowMouse(true);
      }
   }

}
