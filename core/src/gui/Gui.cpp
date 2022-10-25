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

   struct UISliderFloatDesc {
      float min = 0;
      float max = 1;

      void operator()(const char* name, byte* value) {
         ImGui::SliderFloat(name, (float*)value, min, max);
      }
   };

   void UISlider(const char* name, byte* value) {
      UISliderFloatDesc d{};
      std::function<void(const char*, byte*)> uiFunc = d;

      auto sss = UISliderFloatDesc{ .min = 1, .max = 15 };
   }

   void UIColorEdit3(const char* name, byte* value) {
      ImGui::ColorEdit3(name, (float*)value);
   }

   void UIColorPicker3(const char* name, byte* value) {
      ImGui::ColorPicker3(name, (float*)value);
   }

}
