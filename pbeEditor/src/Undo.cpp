#include "pch.h"
#include "Undo.h"
#include "scene/Entity.h"
#include "typer/Serialize.h"

namespace pbe {
   Undo& Undo::Get() {
      static Undo instance;
      return instance;
   }

   void Undo::Delete(const Entity& entity) {
      // todo: dont work with scene hierarchy

      SaveForFuture(entity, false);
      PushSave();
   }

   void Undo::SaveForFuture(const Entity& entity, bool continuous) {
      if (continuous) {
         if (entityContinuous == entity) {
            return;
         } else {
            entityContinuous = entity;
         }
      } else {
         entityContinuous = {};
      }

      // todo: dont work with scene hierarchy

      Serializer ser;
      EntitySerialize(ser, entity);
      string saveData = ser.Str(); // todo: move to lambda

      undoAction = [entity, saveData]() {
         Deserializer deser = Deserializer::FromStr(saveData);
         EntityDeserialize(deser, *entity.GetScene());
      };
   }

   void Undo::PushSave() {
      if (!undoAction) {
         return;
      }

      undoStack.emplace(std::move(undoAction));
      // INFO("Edited"); // todo:
   }

   void Undo::SaveToStack(const Entity& entity, bool continuous) {
      SaveForFuture(entity, continuous);
      PushSave();
   }

}
