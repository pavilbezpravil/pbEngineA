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

}
