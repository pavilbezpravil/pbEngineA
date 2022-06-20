#include "Gui.h"
#include "typer/Typer.h"


void EditorUI(std::string_view name, TypeID typeID, byte* value) {
   Typer::Get().ImGuiValueImpl(name, typeID, value);
}
