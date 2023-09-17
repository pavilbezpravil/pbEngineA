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
   } gTextureViewWindow;

   // todo: add hotkey remove cfg
   CVarSlider<float> zoomScale{ "zoom/scale", 0.05f, 0.f, 1.f };
   CVarValue<bool> cvGizmo{ "gizmo", false };

   struct ViewportSettings {
      vec3 cameraPos{};
      vec2 cameraAngles{};
   };

   constexpr char viewportSettingPath[] = "scene_viewport.yaml";

   STRUCT_BEGIN(ViewportSettings)
      STRUCT_FIELD(cameraPos)
      STRUCT_FIELD(cameraAngles)
   STRUCT_END()

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
      // todo:
      selection->SyncWithScene();

      if (focused) {
         if (Input::IsKeyDown(KeyCode::RightButton)) {
            StartCameraMove();
         }
         if (Input::IsKeyUp(KeyCode::RightButton)) {
            StopCameraMove();
         }
      }

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

      static EditorShowTexture item_current = EditorShowTexture::Lit;
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

            const char* items[] = { "Lit", "Unlit", "Normal", "Roughness", "Metallic", "Diffuse", "Specular",  "Motion" };

            ImGui::SetNextItemWidth(90);
            ImGui::Combo("##Scene RTs", (int*)&item_current, items, IM_ARRAYSIZE(items));
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

      if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && Input::IsKeyDown(KeyCode::LeftButton) && !ImGuizmo::IsOver() && manipulatorMode == None) {
         auto entityID = renderer->GetEntityIDUnderCursor(cmd);

         bool clearPrevSelection = !Input::IsKeyPressing(KeyCode::Shift);
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

         int2 cursorPixelPos{ (mousePos - cursorPos) * renderScale };
         vec2 cursorUV = vec2(cursorPixelPos) / vec2(size);

         cmd.SetCommonSamplers();
         if (scene) {
            renderContext.cursorPixelIdx = cursorPixelPos;

            Entity e = scene->GetAnyWithComponent<CameraComponent>();
            if (!freeCamera && e) {
               auto& trans = e.Get<SceneTransformComponent>();
               camera.position = trans.Position();
               camera.SetViewDirection(trans.Forward());
            }

            renderer->RenderScene(cmd, *scene, camera, renderContext);
         }

         if (item_current != EditorShowTexture::Lit) {
            GPU_MARKER("EditorTexShow");

            cmd.SetRenderTargets();

            auto passDesc = ProgramDesc::Cs("editorTexShow.hlsl", "main");

            Texture2D* texIn = nullptr;
            switch (item_current) {
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
            editorTexShowPass->SetUAV(cmd, "gOut", renderContext.colorLDR);

            cmd.Dispatch2D(renderContext.colorLDR->GetDesc().size, int2{ 8 });
         }

         cmd.pContext->ClearState(); // todo:

         Texture2D* image = renderContext.colorLDR;
         auto srv = image->srv.Get();
         ImGui::Image(srv, imSize);

         static vec2 spawnCursorUV;
         if (Input::IsKeyPressing(KeyCode::Shift) && Input::IsKeyDown(KeyCode::A) && (manipulatorMode & CameraMove) == 0) {
            ImGui::OpenPopup("Add");
            spawnCursorUV = cursorUV;
         }

         {
            UI_PUSH_STYLE_VAR(ImGuiStyleVar_WindowPadding, (ImVec2{ 7, 7 }));
            UI_PUSH_STYLE_VAR(ImGuiStyleVar_PopupRounding, 7 );

            const auto& colorBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
            UI_PUSH_STYLE_COLOR(ImGuiCol_PopupBg, (ImVec4{ colorBg.x, colorBg.y, colorBg.z, 0.75}));

            if (UI_POPUP("Add")) {
               ImGui::Text("Add");
               ImGui::Separator();

               auto spawnPosHint = std::invoke([&]() {
                  auto cursorNDC = (vec2{ 0, 1 } + spawnCursorUV * vec2{ 1, -1 }) * 2.f - 1.f;
                  auto dir4 = camera.GetInvViewProjection() * vec4{ cursorNDC.x, cursorNDC.y, 0, 1 };
                  vec3 dir = dir4 / dir4.w;
                  dir = normalize(dir - camera.position);
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

         // zoom
         if (Input::IsKeyDown(KeyCode::V)) {
            zoomEnable = !zoomEnable;
         }
         if (zoomEnable) {
            Zoom(*image, cursorPixelPos);
         }

         if (cvGizmo) {
            Gizmo(vec2{ imSize.x, imSize.y }, cursorPos);
         } else {
            if (selection->HasSelection() && (manipulatorMode & CameraMove) == 0) {
               auto& dbgRend = *scene->dbgRend;

               Entity entity = selection->FirstSelected();
               auto relativePos = manipulatorRelativeTransform.position;

               vec3 cameraDir = camera.GetWorldSpaceRayDirFromUV(cursorUV);

               Plane plane = Plane::FromPointNormal(relativePos, camera.Forward());
               Ray ray = Ray{ camera.position, cameraDir };

               auto currentPlanePos = plane.RayIntersectionAt(ray);

               if (manipulatorMode == None) {
                  manipulatorRelativeTransform = {
                     entity.GetTransform().Position(),
                     entity.GetTransform().Rotation(),
                     entity.GetTransform().Scale(),
                  };

                  manipulatorInitialPos = currentPlanePos;
               }

               if (Input::IsKeyDown(KeyCode::G)) {
                  manipulatorMode = ObjManipulation | Translate;
                  manipulatorMode |= AxisX | AxisY | AxisZ;
               }
               if (Input::IsKeyDown(KeyCode::R)) {
                  manipulatorMode = ObjManipulation | Rotate;
                  manipulatorMode |= AxisX | AxisY | AxisZ;
               }
               if (Input::IsKeyDown(KeyCode::S)) {
                  manipulatorMode = ObjManipulation | Scale;
                  manipulatorMode |= AxisX | AxisY | AxisZ;
               }

               if (manipulatorMode & ObjManipulation) {
                  if (Input::IsKeyDown(KeyCode::X)) {
                     manipulatorMode &= ~(AxisX | AxisY | AxisZ);
                     manipulatorMode |= AxisX;
                  }
                  if (Input::IsKeyDown(KeyCode::Y)) {
                     manipulatorMode &= ~(AxisX | AxisY | AxisZ);
                     manipulatorMode |= AxisY;
                  }
                  if (Input::IsKeyDown(KeyCode::Z)) {
                     manipulatorMode &= ~(AxisX | AxisY | AxisZ);
                     manipulatorMode |= AxisZ;
                  }

                  dbgRend.DrawSphere(Sphere{ relativePos, 0.03f }, Color_Yellow, false);

                  if (manipulatorMode & (Rotate | Scale)) {
                     dbgRend.DrawLine(relativePos, currentPlanePos, Color_Black, false);
                  }
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

               if (Input::IsKeyDown(KeyCode::LeftButton)) {
                  manipulatorMode = None;
               } else {
                  entity.GetTransform().SetPosition(manipulatorRelativeTransform.position);
                  entity.GetTransform().SetRotation(manipulatorRelativeTransform.rotation);
                  entity.GetTransform().SetScale(manipulatorRelativeTransform.scale);
               }

               if (Input::IsKeyDown(KeyCode::RightButton)) {
                  manipulatorMode = None;
               }

               if (manipulatorMode & Translate) {
                  vec3 translation = currentPlanePos - manipulatorInitialPos;

                  vec3 translationProcessed = vec3{0};

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

               if (manipulatorMode & ObjManipulation) {
                  entity.AddOrReplace<TransformChangedMarker>();
               }
            }
         }

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

      if (manipulatorMode == CameraMove) {
         camera.Update(dt);
      }

      if (manipulatorMode == None) {
         if (Input::IsKeyPressing(KeyCode::Shift)) {
            if (Input::IsKeyDown(KeyCode::D)) {
               auto prevSelected = selection->selected;
               selection->ClearSelection();

               for (auto entity : prevSelected) {
                  auto duplicatedEntity = scene->Duplicate(entity);
                  selection->Select(duplicatedEntity, false);
               }

               manipulatorMode = ObjManipulation | Translate | AllAxis;
            }
         }

         // apply damage
         if (Input::IsKeyPressing(KeyCode::Ctrl)) {
            if (Input::IsKeyDown(KeyCode::D)) {
               for (auto entity : selection->selected) {
                  if (auto destruct = entity.TryGet<DestructComponent>()) {
                     destruct->ApplyDamage(1000.f);
                  }
               }
            }
         }
      }

      if (!Input::IsKeyPressing(KeyCode::RightButton)) {
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

         if (Input::IsKeyDown(KeyCode::F) && selection->FirstSelected()) {
            auto selectedEntity = selection->FirstSelected();
            camera.position = selectedEntity.GetTransform().Position() - camera.Forward() * 3.f;
            camera.UpdateView();
         }
      }

      if (Input::IsKeyDown(KeyCode::X)) { // todo: other key
         freeCamera = !freeCamera;
      }

      if (Input::IsKeyDown(KeyCode::C)) { // todo: other key
         if (Entity e = selection->FirstSelected()) {
            e.Get<SceneTransformComponent>().SetPosition(camera.position);
            // todo: set rotation
         }
      }

      // todo:
      static TimedAction timer{5};
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
               .scale = vec3_One * 0.5f,
            });

         auto& rb = shoot.Get<RigidBodyComponent>();
         rb.SetLinearVelocity(camera.Forward() * 25.f);
      }
   }

   void ViewportWindow::OnLostFocus() {
      StopCameraMove();
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

      bool snap = Input::IsKeyPressing(KeyCode::Ctrl);

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

         selectedEntity.AddOrReplace<TransformChangedMarker>();
      }
   }

   void ViewportWindow::StartCameraMove() {
      if ((manipulatorMode & CameraMove) == 0 && (manipulatorMode & ObjManipulation) == 0) {
         manipulatorMode = CameraMove;
         Input::HideMouse(true);
      }
   }

   void ViewportWindow::StopCameraMove() {
      if (manipulatorMode == CameraMove) {
         ASSERT((manipulatorMode & ObjManipulation) == 0);
         manipulatorMode = None;
         Input::ShowMouse(true);
      }
   }

}
