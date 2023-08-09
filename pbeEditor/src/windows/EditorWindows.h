#pragma once

#include "EditorWindow.h"
#include "core/CVar.h"

namespace pbe {

   class ProfilerWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;
      void OnWindowUI() override;
   };

   class ConfigVarsWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;
      void OnWindowUI() override;
      void CVarChilds(const ConfigVarsMng::CVarChilds& childs);
   };

   class ShaderWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;
      void OnWindowUI() override;
   };

}
