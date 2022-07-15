#pragma once
#include <string>

#include "core/Common.h"

namespace pbe {

   class EditorWindow {
      NON_COPYABLE(EditorWindow);
   public:
      EditorWindow(std::string_view name) : name(name) { }
      virtual ~EditorWindow() = default;

      virtual void OnImGuiRender() = 0;
      virtual void OnUpdate(float dt) {}

      std::string name{ "EditorWindow" };
      bool show = true;
   };

}
