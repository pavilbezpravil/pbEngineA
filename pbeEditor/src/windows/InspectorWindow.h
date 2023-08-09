#pragma once

#include "EditorWindow.h"

namespace pbe {

   struct EditorSelection;

   class InspectorWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;
      void OnWindowUI() override;

      EditorSelection* selection{};
   };

}
