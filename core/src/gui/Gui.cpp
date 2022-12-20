#include "pch.h"
#include "Gui.h"
#include "typer/Typer.h"

namespace pbe {

   ImGuiContext* GetImGuiContext() {
      return ImGui::GetCurrentContext();
   }

   void EditorUI(std::string_view name, TypeID typeID, byte* value) {
      Typer::Get().ImGuiValueImpl(name, typeID, value);
   }

   void UIColorEdit3(const char* name, byte* value) {
      ImGui::ColorEdit3(name, (float*)value);
   }

   void UIColorPicker3(const char* name, byte* value) {
      ImGui::ColorPicker3(name, (float*)value);
   }

}
