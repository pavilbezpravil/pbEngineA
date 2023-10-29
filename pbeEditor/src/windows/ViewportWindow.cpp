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
#include "math/Common.h"
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

      STRUCT_FIELD(useSnap)
      STRUCT_FIELD(snapSpace)
      STRUCT_FIELD(snapTranslationScale)

      STRUCT_FIELD(useGizmo)
      STRUCT_FIELD(space)
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
      auto startCursorPos = ImGui::GetCursorPos();
      auto contentRegion = ImGui::GetContentRegionAvail();
      int2 size = int2{ contentRegion.x, contentRegion.y };

      if (!all(size > 1)) {
         return;
      }

      size = vec2{ contentRegion.x, contentRegion.y } * settings.renderScale;
      size = glm::max(size, int2{ 1 });

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

      if (state == ViewportState::None && Input::IsKeyDown(KeyCode::LeftButton) && ImGui::IsWindowHovered() && !ImGuizmo::IsOver()) {
         if (Input::IsKeyPressing(KeyCode::Ctrl)) {
            ApplyDamageFromCamera(camera.GetWorldSpaceRayDirFromUV(cursorUV));
         } else {
            SelectEntity((EntityID)renderer->GetEntityIDUnderCursor(cmd));
         }
      }

      cmd.SetCommonSamplers();
      if (scene) {
         renderContext.cursorPixelIdx = cursorPixelIdx;

         Entity cameraEntity = Entity::GetAnyWithComponent<CameraComponent>(*scene);
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

      if (settings.useGizmo) {
         Gizmo(ToVec2(contentRegion), cursorPos);
      } else {
         Manipulator(cursorUV);
      }

      if (state == ViewportState::None) {
         ViewportToolbar(startCursorPos);
      }

      float hotKeyBarHeight = HotKeyBar();

      {
         ImGui::SetCursorPos({ 0, 0 });
         auto contentRegion = ImGui::GetContentRegionAvail();
         notifyManager.UI(contentRegion.y - hotKeyBarHeight);
      }
   }

   void ViewportWindow::OnUpdate(float dt) {
      // todo:
      if (!focused) {
         return;
      }

      camera.NextFrame(); // todo:

      if (state == ViewportState::CameraMove) {
         if (Input::IsKeyUp(KeyCode::RightButton)) {
            StopCameraMove();
         }

         camera.Update(dt);
      }

      if (state == ViewportState::None) {
         if (Input::IsKeyDown(KeyCode::RightButton)) {
            StartCameraMove();
         }

         if (selection->HasSelection()) {
            if (Input::IsKeyPressing(KeyCode::Shift)) {
               if (Input::IsKeyDown(KeyCode::D)) {
                  notifyManager.AddNotify("Duplicated");

                  auto prevSelected = selection->selected;
                  selection->ClearSelection();

                  for (auto entity : prevSelected) {
                     auto duplicatedEntity = scene->Duplicate(entity);
                     selection->Select(duplicatedEntity, false);
                  }

                  StartManipulator(Translate);
               }
            }

            if (Input::IsKeyDown(KeyCode::E)) {
               notifyManager.AddNotify("Unselected parent and select children");

               Entity lastSelected = selection->LastSelected();

               if (Input::IsKeyPressing(KeyCode::Shift)) {
                  selection->Unselect(lastSelected);
               } else {
                  selection->ClearSelection();
               }

               for (auto& childEntity : lastSelected.GetTransform().children) {
                  selection->Select(childEntity, false);
               }
            }

            if (Input::IsKeyDown(KeyCode::Q)) {
               notifyManager.AddNotify("Selected parent");

               auto parent = selection->LastSelected().GetTransform().parent;
               if (parent.GetTransform().parent) { // not root
                  selection->Select(parent, !Input::IsKeyPressing(KeyCode::Shift));
               }
            }

            if (Input::IsKeyDown(KeyCode::F)) {
               notifyManager.AddNotify("Focused");

               auto selectedEntity = selection->LastSelected();
               camera.position = selectedEntity.GetTransform().Position() - camera.Forward() * 3.f;
               camera.UpdateView();
            }

            if (Input::IsKeyDown(KeyCode::X)) {
               notifyManager.AddNotify("Deleted");

               while (selection->HasSelection()) {
                  Entity entity = selection->LastSelected();
                  entity.DestroyDelayed();
                  selection->Unselect(selection->LastSelected());
               }
            }

            if (Input::IsKeyDown(KeyCode::H)) {
               bool disable = selection->LastSelected().Enabled();

               notifyManager.AddNotify(disable ? "Disable" : "Enable");

               for (Entity& entity : selection->selected) {
                  entity.Enable(!disable);
               }
            }

            if (Input::IsKeyDown(KeyCode::C)) { // todo: other key
               notifyManager.AddNotify("Set camera position");

               Entity e = selection->LastSelected();
               e.Get<SceneTransformComponent>().SetPosition(camera.position);
               // todo: set rotation
            }
         }

         // todo: find suitable hot key
         // if (Input::IsKeyDown(KeyCode::Q)) {
         //    settings.space = 1 - settings.space;
         // }

         // freeCamera = !freeCamera;
      }

      if (state == ViewportState::None || state == ViewportState::CameraMove) {
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
      if (state == ViewportState::None) {
         if (Input::IsKeyDown(KeyCode::W)) {
            gizmoCfg.operation = ImGuizmo::OPERATION::TRANSLATE;
         }
         if (Input::IsKeyDown(KeyCode::R)) {
            gizmoCfg.operation = ImGuizmo::OPERATION::ROTATE;
         }
         if (Input::IsKeyDown(KeyCode::S)) {
            gizmoCfg.operation = ImGuizmo::OPERATION::SCALE;
         }
      }

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
         (ImGuizmo::MODE)settings.space,
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

   void ViewportWindow::ViewportToolbar(const ImVec2& cursorPos) {
      ImGui::SetCursorPos(cursorPos + ImVec2{ 4, 4 });
      {
         UI_PUSH_STYLE_COLOR(ImGuiCol_Button, (ImVec4{ 0, 0, 0, 0.2f }));
         UI_PUSH_STYLE_COLOR(ImGuiCol_ButtonActive, (ImVec4{ 0, 0, 0, 0.4f }));
         UI_PUSH_STYLE_COLOR(ImGuiCol_ButtonHovered, (ImVec4{ 0, 0, 0, 0.3f }));

         if (ImGui::Button(settings.showToolbar ? "<" : ">")) {
            settings.showToolbar = !settings.showToolbar;
         }
      }

      if (!settings.showToolbar) {
         return;
      }

      ImGui::SameLine();

      UI_PUSH_STYLE_COLOR(ImGuiCol_ChildBg, (ImVec4{ 0, 0, 0, 0.3f }));
      UI_PUSH_STYLE_VAR(ImGuiStyleVar_ChildRounding, 10);

      float windowWidth = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x;

      if (UI_CHILD_WINDOW("Viewport tools", (ImVec2{ windowWidth, ImGui::GetFrameHeight()}))) {
         UI_PUSH_STYLE_VAR(ImGuiStyleVar_FrameBorderSize, 1);
         UI_PUSH_STYLE_VAR(ImGuiStyleVar_FrameRounding, 10);

         ImGui::SetNextItemWidth(100);
         const char* sceneRTs[] = { "Lit", "Unlit", "Normal", "Roughness", "Metallic", "Diffuse", "Specular",  "Motion" };
         ImGui::Combo("##Scene RTs", &settings.showedTexIdx, sceneRTs, IM_ARRAYSIZE(sceneRTs));
         ImGui::SameLine();

         ImGui::SetNextItemWidth(100);
         ImGui::SliderFloat("Scale", &settings.renderScale, 0.1f, 1.f);
         ImGui::SameLine();

         ImGui::Checkbox("RT", &settings.rayTracingRendering);
         rayTracingSceneRender = settings.rayTracingRendering;
         ImGui::SameLine();

         ImGui::Checkbox("Snap", &settings.useSnap);
         ImGui::SameLine();

         if (settings.useSnap) {
            ImGui::SetNextItemWidth(100);
            const char* spaces[] = { "Local", "World" };
            ImGui::Combo("##Snap Space", &settings.snapSpace, spaces, IM_ARRAYSIZE(spaces));
            ImGui::SameLine();

            ImGui::SetNextItemWidth(100);
            ImGui::SliderFloat("Snap value", &settings.snapTranslationScale, 0.01f, 0.2f, "%.2f");
            ImGui::SameLine();
         }

         ImGui::Checkbox("Gizmo", &settings.useGizmo);
         ImGui::SameLine();

         if (settings.useGizmo) {
            ImGui::SetNextItemWidth(100);
            const char* spaces[] = { "Local", "World" };
            ImGui::Combo("##Transform Space", &settings.space, spaces, IM_ARRAYSIZE(spaces));
            ImGui::SameLine();
         }

         constexpr const char* visualizePopupName = "Visualize Popup";
         if (ImGui::Button("Visualize")) {
            ImGui::OpenPopup(visualizePopupName);
         }

         if (UI_POPUP(visualizePopupName, ImGuiWindowFlags_MenuBar)) {
            if (UI_MENU("Physics")) {
               static bool showPhysics = true;
               static bool showJoints = true;

               if (ImGui::Selectable("Joints", &showJoints, ImGuiSelectableFlags_DontClosePopups)) {
                  
               }
               if (ImGui::Selectable("Destruction", &showPhysics, ImGuiSelectableFlags_DontClosePopups)) {
                  
               }
            }
         }
      }
   }

   float ViewportWindow::HotKeyBar() {
      float barPadding = 5;

      UI_PUSH_STYLE_COLOR(ImGuiCol_ChildBg, (ImVec4{ 0.5, 0.5, 0.5, 0.5f }));
      UI_PUSH_STYLE_VAR(ImGuiStyleVar_WindowPadding, (ImVec2{ barPadding, barPadding }));

      ImGui::SetCursorPos({ 0, 0 });
      auto contentRegion = ImGui::GetContentRegionAvail();

      ImVec2 barSize = { contentRegion.x, ImGui::GetFrameHeightWithSpacing() };

      ImGui::SetCursorPos(ImVec2{ 0, contentRegion.y - barSize.y });

      if (UI_CHILD_WINDOW("HotKey bar", barSize, false, ImGuiWindowFlags_AlwaysUseWindowPadding)) {
         switch (state) {
            case ViewportState::None:
               if (!Input::IsKeyPressing(KeyCode::Ctrl)) {
                  ImGui::Text("LM - select");
                  ImGui::SameLine();
               }

               ImGui::Text("RM - camera move");
               ImGui::SameLine();

               if (selection->HasSelection()) {
                  ImGui::Text("G/R/S - translation/rotation/scale");
                  ImGui::SameLine();

                  ImGui::Text("Q - select parent");
                  ImGui::SameLine();

                  ImGui::Text("H - disable");
                  ImGui::SameLine();

                  ImGui::Text("X - delete");
                  ImGui::SameLine();

                  ImGui::Text("F - focus");
                  ImGui::SameLine();

                  ImGui::Text("C - set camera position");
                  ImGui::SameLine();
               }

               ImGui::Text("Ctrl + LM - apply damage");
               ImGui::SameLine();
               break;
            case ViewportState::CameraMove:
               ImGui::Text("W/A/S/D - move");
               ImGui::SameLine();
               ImGui::Text("Q/E - up/down");
               ImGui::SameLine();
               break;
            case ViewportState::ObjManipulation:
               ImGui::Text("LM - confirm");
               ImGui::SameLine();

               ImGui::Text("RM - cancel");
               ImGui::SameLine();

               if ((manipulatorMode & Translate) == 0) {
                  ImGui::Text("G - translate");
                  ImGui::SameLine();
               } else if ((manipulatorMode & Rotate) == 0) {
                  ImGui::Text("R - rotate");
                  ImGui::SameLine();
               } else if ((manipulatorMode & Scale) == 0) {
                  ImGui::Text("S - scale");
                  ImGui::SameLine();
               }

               ImGui::Text("world space");
               ImGui::SameLine();

               bool allAxisMode = (manipulatorMode & AllAxis) == AllAxis;

               if ((manipulatorMode & AxisX) == 0 || allAxisMode) {
                  ImGui::Text("X - x axis");
                  ImGui::SameLine();
               }

               if ((manipulatorMode & AxisX) == 0 || allAxisMode) {
                  ImGui::Text("Y - y axis");
                  ImGui::SameLine();
               }

               if ((manipulatorMode & AxisX) == 0 || allAxisMode) {
                  ImGui::Text("Y - y axis");
                  ImGui::SameLine();
               }

               break;
         }
      }

      return barSize.y;
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
            notifyManager.AddNotify("Add entity");
         }
      }
   }

   void ViewportWindow::Manipulator(const vec2& cursorUV) {
      if (!selection->HasSelection() || state == ViewportState::CameraMove) {
         StopManipulator();
         return;
      }

      if (Input::IsKeyDown(KeyCode::G)) {
         StartManipulator(Translate);
      }
      if (Input::IsKeyDown(KeyCode::R)) {
         StartManipulator(Rotate);
      }
      if (Input::IsKeyDown(KeyCode::S)) {
         StartManipulator(Scale);
      }

      if (state != ViewportState::ObjManipulation) {
         return;
      }

      auto relativePos = manipulatorRelativeTransform.position;

      vec3 cameraDir = camera.GetWorldSpaceRayDirFromUV(cursorUV);

      Plane billboardPlane = Plane::FromPointNormal(relativePos, camera.Forward());
      Ray ray = Ray{ camera.position, cameraDir };

      auto currentPlanePos = billboardPlane.RayIntersectionAt(ray);

      bool startManipulate = manipulatorMode & RequestStart;
      manipulatorMode &= ~RequestStart;

      if (startManipulate) {
         manipulatorInitialBillboardPos = currentPlanePos;
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

      auto& dbgRend = *scene->dbgRend;

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

      bool useSnap = settings.useSnap;
      if (Input::IsKeyPressing(KeyCode::Ctrl)) {
         useSnap = !useSnap;
      }

      if (manipulatorMode & Translate) {
         vec3 translation = currentPlanePos - manipulatorInitialBillboardPos;

         if ((manipulatorMode & AllAxis) != AllAxis) {
            vec3 posOnBillboardPlane = manipulatorRelativeTransform.position + translation;
            vec3 rayFromCam = glm::normalize(posOnBillboardPlane - camera.position);

            vec3 axis = vec3_Zero;
            if (manipulatorMode & AxisX) {
               axis = vec3_X;
            }
            if (manipulatorMode & AxisY) {
               axis = vec3_Y;
            }
            if (manipulatorMode & AxisZ) {
               axis = vec3_Z;
            }

            vec3 planeNormal = glm::normalize(glm::cross(axis, camera.Forward()));
            planeNormal = glm::normalize(glm::cross(axis, planeNormal));

            Plane alongPlane = Plane::FromPointNormal(relativePos, planeNormal);
            Ray rayToBillboardPos = Ray{ camera.position, rayFromCam };

            auto alongIntersectPos = alongPlane.RayIntersectionAt(rayToBillboardPos);

            translation = dot(alongIntersectPos - relativePos, axis) * axis;
         }

         for (Entity& entity : selection->selected) {
            auto it = selectedEntityInitialTransforms.find(entity.GetEntityID());
            if (it == selectedEntityInitialTransforms.end()) {
               continue;
            }

            auto& trans = entity.GetTransform();
            trans.SetPosition(it->second.position + translation);

            if (useSnap) {
               vec3 position = trans.Position((Space)settings.snapSpace);
               position = glm::round(position / settings.snapTranslationScale) * settings.snapTranslationScale;
               trans.SetPosition(position, (Space)settings.snapSpace);
            }
         }
      }

      if (manipulatorMode & Rotate) {
         vec3 initialDir = normalize(manipulatorInitialBillboardPos - relativePos);
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

         for (Entity& entity : selection->selected) {
            auto it = selectedEntityInitialTransforms.find(entity.GetEntityID());
            if (it == selectedEntityInitialTransforms.end()) {
               continue;
            }

            auto& trans = entity.GetTransform();

            quat rotation = glm::angleAxis(angle, axis);
            trans.SetRotation(rotation * it->second.rotation);
         }
      }

      if (manipulatorMode & Scale) {
         float initialDistance = glm::distance(manipulatorInitialBillboardPos, relativePos);
         float currentDistance = glm::distance(currentPlanePos, relativePos);
         float scale = currentDistance / initialDistance;

         vec3 scale3 = vec3{
            manipulatorMode & AxisX ? scale : 1.f,
            manipulatorMode & AxisY ? scale : 1.f,
            manipulatorMode & AxisZ ? scale : 1.f,
         };

         for (Entity& entity : selection->selected) {
            auto it = selectedEntityInitialTransforms.find(entity.GetEntityID());
            if (it == selectedEntityInitialTransforms.end()) {
               continue;
            }

            auto& trans = entity.GetTransform();
            trans.SetScale(it->second.scale * scale3);

            if (useSnap) {
               vec3 scale = trans.Scale((Space)settings.snapSpace);
               scale = glm::round(scale / settings.snapTranslationScale) * settings.snapTranslationScale;
               trans.SetScale(scale, (Space)settings.snapSpace);
            }
         }
      }

      for (Entity& entity : selection->selected) {
         auto it = selectedEntityInitialTransforms.find(entity.GetEntityID());
         if (it == selectedEntityInitialTransforms.end()) {
            continue;
         }

         entity.AddOrReplace<TransformChangedMarker>();
      }

      if (Input::IsKeyDown(KeyCode::LeftButton)) {
         state = ViewportState::None;
      }
   }

   void ViewportWindow::ManipulatorResetTransforms() {
      if (state != ViewportState::ObjManipulation || !selection->HasSelection()) {
         return;
      }

      for (Entity& entity : selection->selected) {
         auto it = selectedEntityInitialTransforms.find(entity.GetEntityID());
         if (it == selectedEntityInitialTransforms.end()) {
            continue;
         }

         entity.GetTransform().SetTransform(it->second);
         entity.AddOrReplace<TransformChangedMarker>();
      }
   }

   void ViewportWindow::StartManipulator(ManipulatorMode mode) {
      if (state == ViewportState::ObjManipulation) {
         manipulatorMode &= ~(Translate | Rotate | Scale);
         manipulatorMode |= mode;
         return;
      }

      if (state != ViewportState::None) {
         return;
      }

      state = ViewportState::ObjManipulation;
      manipulatorMode = mode | AllAxis | RequestStart;

      selectedEntityInitialTransforms.clear();
      for (auto entity : selection->selected) {
         selectedEntityInitialTransforms[entity.GetEntityID()] = entity.GetTransform().World();
      }

      // todo: add average
      manipulatorRelativeTransform = selection->LastSelected().GetTransform().World();
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
