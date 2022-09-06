#include "pch.h"
#include "CVar.h"

namespace pbe {

   ConfigVarsMng sConfigVarsMng;

   CVar::CVar(std::string_view name) : name(name) {
      sConfigVarsMng.configVars.push_back(this);
   }

   void ConfigVarsMng::NextFrame() {
      for (auto* cvar : configVars) {
         cvar->NextFrame();
      }
   }

   void CVarValue<bool>::UI() {
      ImGui::Checkbox(name.c_str(), &value);
   }

   void CVarValue<int>::UI() {
      ImGui::InputInt(name.c_str(), &value);
   }

   void CVarValue<float>::UI() {
      ImGui::InputFloat(name.c_str(), &value);
   }

   void CVarSlider<int>::UI() {
      ImGui::SliderInt(name.c_str(), &value, min, max);
   }

   void CVarSlider<float>::UI() {
      ImGui::SliderFloat(name.c_str(), &value, min, max);
   }

   CVarTrigger::CVarTrigger(std::string_view name): CVar(name) {
   }

   void CVarTrigger::UI() {
      // triggered = ImGui::Button("Trigger");
      if (ImGui::Button("Trigger")) {
         triggered = true;
      }
      ImGui::SameLine();
      ImGui::Text(name.c_str());
   }

   void CVarTrigger::NextFrame() {
      triggered = false;
   }

}
