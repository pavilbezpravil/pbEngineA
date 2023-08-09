#pragma once
#include <string>

#include "core/Common.h"

namespace pbe {

   class EditorWindow {
      NON_COPYABLE(EditorWindow);
   public:
      EditorWindow(std::string_view name) : name(name) { }
      virtual ~EditorWindow() = default;

      virtual void OnBefore() {}
      virtual void OnWindowUI() = 0;
      virtual void OnAfter() {}

      virtual void OnUpdate(float dt) {}

      std::string name{ "EditorWindow" };
      bool show = true;
   };

}
