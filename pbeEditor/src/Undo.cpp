#include "pch.h"
#include "Undo.h"
#include "scene/Entity.h"

namespace pbe {
   Undo& Undo::Get()
   {
      static Undo instance;
      return instance;
   }

   void Undo::Mark(ComponentInfo& ci, const Entity& entity) {
      Push([&ci, entity]() mutable {
         auto* pComponent = ci.tryGet(entity);

         auto deser = Deserializer::FromStr(serPrev.Str());
         // deser.Deser("", ci.typeID, data.data());
         deser.Deser("", ci.typeID, (byte*)pComponent);
         // ci.replace(entity, deser);

         // auto* pComponent = ci.tryGet(entity);
         // ci.duplicate(pComponent, data.data());
      });
   }
}
