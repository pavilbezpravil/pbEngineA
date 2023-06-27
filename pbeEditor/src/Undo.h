#pragma once
#include "typer/Typer.h"
#include "scene/Entity.h"

namespace pbe {

   // class Entity;

   class Undo {
   public:
      static Undo& Get();

      // todo: bad naming

      void Delete(const Entity& entity);

      void SaveForFuture(const Entity& entity, bool continuous = false);
      void PushSave();

      void SaveToStack(const Entity& entity, bool continuous = false);

      // todo: name
      void PopAction() {
         if (!undoStack.empty()) {
            auto undoFunc = undoStack.top();
            undoFunc();
            undoStack.pop();
         }
      }

   private:
      std::stack<std::function<void()>> undoStack;
      std::function<void()> undoAction;

      Entity entityContinuous;
   };

}
