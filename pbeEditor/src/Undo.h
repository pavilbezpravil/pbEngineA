#pragma once
#include "typer/Typer.h"

namespace pbe {

   class Entity;

   class Undo {
   public:
      static Undo& Get();

      void Mark(ComponentInfo& ci, const Entity& entity);

      void Push(std::function<void()> undoAction) {
         // todo: add move
         undoStack.push(undoAction);
      }

      // todo: name
      void PopAction() {
         if (!undoStack.empty()) {
            auto undoFunc = undoStack.top();
            undoFunc();
            // undoStack.top()();
            undoStack.pop();
         }
      }

   private:
      std::stack<std::function<void()>> undoStack;
   };

}
