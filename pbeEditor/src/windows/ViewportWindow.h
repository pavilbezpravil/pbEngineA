#pragma once

#include "EditorCamera.h"
#include "EditorSelection.h"
#include "EditorWindow.h"
#include "math/Transform.h"
#include "rend/Renderer.h"

namespace pbe {

   class Scene;
   class Texture2D;

   struct GizmoCfg {
      int operation = 7; // translate
      float snap = 1;
   };

   struct ViewportSettings {
      vec3 cameraPos{};
      vec2 cameraAngles{};

      bool showToolbar = true;
      float renderScale = 1.f;
      int showedTexIdx = 0;
      bool rayTracingRendering = false;

      bool useGizmo = true; // instead of manipulator
      int space = 1; // world
   };

   class ViewportWindow : public EditorWindow {
   public:
      // using EditorWindow::EditorWindow;
      ViewportWindow(std::string_view name);
      ~ViewportWindow();

      void OnBefore() override;
      void OnAfter() override;
      void OnWindowUI() override;

      void OnUpdate(float dt) override;

      void OnLostFocus() override;

      void Zoom(Texture2D& image, vec2 center);
      void Gizmo(const vec2& contentRegion, const vec2& cursorPos);

      Scene* scene{};
      EditorSelection* selection{};

      EditorCamera camera;
      Renderer* renderer = {}; // todo:
      RenderContext renderContext;

      bool freeCamera = true;

      GizmoCfg gizmoCfg;

      // todo: add enum ViewportState

      enum class ViewportState {
         None,
         ObjManipulation,
         CameraMove,
      };

      enum ManipulatorMode {
         None = 0,
         Translate = BIT(1),
         Rotate = BIT(2),
         Scale = BIT(3),
         AxisX = BIT(4),
         AxisY = BIT(5),
         AxisZ = BIT(6),
      };

   private:
      ManipulatorMode AllAxis = ManipulatorMode(AxisX | AxisY | AxisZ);

      ViewportSettings settings;

      bool zoomEnable = false;

      Transform manipulatorRelativeTransform;
      vec3 manipulatorInitialPos;

      ViewportState state = ViewportState::None;
      ManipulatorMode manipulatorMode = ManipulatorMode::None;

      void ViewportToolbar(const ImVec2& cursorPos);
      void NotifyPanel();

      void ApplyDamageFromCamera(const vec3& rayDirection);
      void SelectEntity(EntityID entityID);

      void DrawTexture(CommandList& cmd, Texture2D& image, const ImVec2& imSize);
      void AddEntityPopup(const vec2& cursorUV);

      void Manipulator(const vec2& cursorUV);

      void ManipulatorResetTransforms();
      void StopManipulator();

      void StartCameraMove();
      void StopCameraMove();
   };

   DEFINE_ENUM_FLAG_OPERATORS(ViewportWindow::ManipulatorMode)

}
