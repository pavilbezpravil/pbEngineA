#include "pch.h"
#include "Component.h"

#include "gui/Gui.h"
#include "typer/Typer.h"
#include "scene/Entity.h"

namespace pbe {

   COMPONENT_EXPLICIT_TEMPLATE_DEF(UUIDComponent);

   TYPER_BEGIN(TagComponent)
      TYPER_FIELD(tag)
   TYPER_END(TagComponent)

   COMPONENT_EXPLICIT_TEMPLATE_DEF(TagComponent);

   TYPER_BEGIN(SceneTransformComponent)
      TYPER_FIELD(position)
      TYPER_FIELD(rotation)
      TYPER_FIELD(scale)
   TYPER_END(SceneTransformComponent)

   COMPONENT_EXPLICIT_TEMPLATE_DEF(SceneTransformComponent)

   TYPER_BEGIN(SimpleMaterialComponent)
      TYPER_FIELD(albedo)
      TYPER_FIELD(roughness)
      TYPER_FIELD(metallic)
   TYPER_END(SimpleMaterialComponent)

   COMPONENT_EXPLICIT_TEMPLATE_DEF(SimpleMaterialComponent)

   ComponentList& ComponentList::Get() {
      static ComponentList cl;
      return cl;
   }

   void ComponentList::RegisterComponent(ComponentID componentID, ComponentFunc&& func) {
      components2[componentID] = std::move(func);
   }

   int RegisterComponents() {
      TypeID id;

      id = GetTypeID<SceneTransformComponent>();
      ComponentList::Get().RegisterComponent(id, [](Entity& entity) {
         if (auto* c = entity.TryGet<SceneTransformComponent>()) {
            EditorUI<SceneTransformComponent>(c->GetName(), *c);
         }
         });

      id = GetTypeID<SimpleMaterialComponent>();
      ComponentList::Get().RegisterComponent(id, [](Entity& entity) {
         if (auto* c = entity.TryGet<SimpleMaterialComponent>()) {
            EditorUI<SimpleMaterialComponent>(c->GetName(), *c);
         }
         });

      return 0;
   }

   static int __unused_value = RegisterComponents();

}
