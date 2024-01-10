#pragma once
#include "Entity.h"


namespace pbe {

   template<typename F>
   concept FuncTakesEntity = requires(F func, pbe::Entity & entity) {
      func(entity);
   };

   template<typename F>
   concept FuncTakesConstEntity = requires(F func, const Entity & entity) {
      func(entity);
   };

   struct SceneHier {
      template<typename F> requires FuncTakesEntity<F>
      static void ApplyFuncForChildren(Entity root, F&& func, bool applyOnRoot = true) {
         if (applyOnRoot) {
            func(root);
         }

         auto& trans = root.GetTransform();
         for (auto& childEntity : trans.children) {
            ApplyFuncForChildren(childEntity, std::forward<F>(func));
         }
      }

      // requires requires (F func, const Entity& entity) { func(entity); }
      template<typename F> requires FuncTakesConstEntity<F>
      static Entity FindParentWith(const Entity& entity, F&& pred) {
         const auto& trans = entity.GetTransform();

         const auto& parent = trans.parent;
         if (!parent || pred(parent)) {
            return parent;
         }

         return FindParentWith(parent, std::forward<F>(pred));
      }

      template<typename Comp>
      static Entity FindParentWithComponent(const Entity& entity) {
         return FindParentWith(entity, [](const Entity& entity) { return entity.Has<Comp>(); });
      }
   };

}
