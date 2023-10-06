#pragma once

#include "core/Core.h"
#include "math/Types.h"

namespace pbe {

   class CORE_API CVar {
   public:
      CVar(std::string_view fullPath);

      virtual void UI() = 0;
      virtual void NextFrame() {}

   // private:
      std::string fullPath;
      std::string name;
   };

   class CORE_API ConfigVarsMng {
   public:
      struct CVarChilds {
         std::string folderName;
         CVar* cvar = nullptr;
         std::vector<CVarChilds> childs;

         bool IsLeaf() const { return childs.empty(); }
      };

      void AddCVar(CVar* cvar);
      void NextFrame();

      const CVarChilds& GetCVarsRoot() const { return root; }

      const std::vector<CVar*>& GetCVars() const { return configVars; }
   private:
      bool cvarsChanged = false;

      std::vector<CVar*> configVars;
      CVarChilds root;
   };

   extern ConfigVarsMng CORE_API sConfigVarsMng;

   template<typename T>
   class CVarValue : public CVar {
   public:
      CVarValue(std::string_view name, T initialValue)
            : CVar(name), value(initialValue) {
      }

      CVarValue& operator= (T newValue) {
         value = newValue;
         return *this;
      }
      operator T() const { return value; }

      void UI() override;

   private:
      T value = false;
   };

   extern template void CORE_API CVarValue<bool>::UI();
   extern template void CORE_API CVarValue<int>::UI();
   extern template void CORE_API CVarValue<uint>::UI();
   extern template void CORE_API CVarValue<float>::UI();

   template<typename T>
   class CVarSlider : public CVar {
   public:
      CVarSlider(std::string_view name, T initialValue, T min, T max)
               : CVar(name), value(initialValue), min(min), max(max) {
      }

      operator T() const { return value; }

      void UI() override;

   private:
      T value;
      T min;
      T max;
   };

   extern template void CORE_API CVarSlider<int>::UI();
   extern template void CORE_API CVarSlider<float>::UI();

   class CVarTrigger : public CVar {
   public:
      CVarTrigger(std::string_view name);

      operator bool() const { return triggered; }

      void UI() override;
      void NextFrame() override;

   private:
      bool triggered = false;
   };

}
