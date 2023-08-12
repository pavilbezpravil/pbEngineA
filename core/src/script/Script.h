#pragma once

#include "scene/Entity.h"


namespace pbe {

   class CORE_API Script {
   public:
      virtual ~Script() = default;

      virtual void OnEnable() {}
      virtual void OnDisable() {}

      virtual void OnUpdate(float dt) {}

      const char* GetName() const;

      Scene& GetScene() const { return *owner.GetScene(); }

      Entity owner;
   };

}
