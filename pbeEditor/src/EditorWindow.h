#pragma once
#include <string>

#include "core/Common.h"

class EditorWindow {
   NON_COPYABLE(EditorWindow);
public:
   EditorWindow(std::string_view name) : name(name) {}
   virtual ~EditorWindow() = default;

   virtual void OnImGuiRender() = 0;

   std::string name{ "EditorWindow" };
};
