#pragma once

#include "core/Core.h"

namespace pbe {

   class CORE_API CVar {
   public:
      CVar(std::string_view name);

      virtual void UI() = 0;

   // private:
      std::string name;
   };

   class CORE_API ConfigVarsMng {
   public:
      std::vector<CVar*> configVars;
   };

   extern ConfigVarsMng CORE_API sConfigVarsMng;

   template<typename T>
   class CVarValue : public CVar {
   public:
      CVarValue(std::string_view name, T initialValue)
            : CVar(name), value(initialValue) {
      }

      operator T() const { return value; }

      void UI() override;

   private:
      T value = false;
   };

   extern template void CORE_API CVarValue<bool>::UI();
   extern template void CORE_API CVarValue<int>::UI();

   template<typename T>
   class CVarSlider : public CVar {
   public:
      CVarSlider(std::string_view name, T initialValue, T min, T max)
               : CVar(name), value(initialValue), min(min), max(max) {
      }

      void UI() override;

   private:
      T value;
      T min;
      T max;
   };

   extern template void CORE_API CVarSlider<int>::UI();

}
