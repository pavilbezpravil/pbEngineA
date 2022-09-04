#include "pch.h"
#include "CVar.h"

namespace pbe {

   ConfigVarsMng sConfigVarsMng;

   CVar::CVar(std::string_view name) : name(name) {
      sConfigVarsMng.configVars.push_back(this);
   }

   void CVarValue<bool>::UI() {
      ImGui::Checkbox(name.c_str(), &value);
   }

   void CVarValue<int>::UI() {
      ImGui::InputInt(name.c_str(), &value);
   }

   void CVarSlider<int>::UI() {
      ImGui::SliderInt(name.c_str(), &value, min, max);
   }

}
