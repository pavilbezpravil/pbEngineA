#include "pch.h"
#include "Gui.h"
#include "typer/Typer.h"

namespace pbe {

   ImGuiContext* GetImGuiContext() {
      return ImGui::GetCurrentContext();
   }

   bool EditorUI(std::string_view name, TypeID typeID, byte* value) {
      return Typer::Get().ImGuiValueImpl(name, typeID, value);
   }

   bool UIColorEdit3(const char* name, byte* value) {
      return ImGui::ColorEdit3(name, (float*)value);
   }

   bool UIColorPicker3(const char* name, byte* value) {
      return ImGui::ColorPicker3(name, (float*)value);
   }

}
