#include "Component.h"

#include "gui/Gui.h"
#include "typer/Typer.h"
#include "scene/Entity.h"

namespace pbe {

   COMPONENT_EXPLICIT_TEMPLATE_DEF(UUIDComponent);
   COMPONENT_EXPLICIT_TEMPLATE_DEF(TagComponent);

   TYPER_BEGIN(SceneTransformComponent)
      TYPER_FIELD(position)
      TYPER_FIELD(rotation)
   TYPER_END(SceneTransformComponent)

   COMPONENT_EXPLICIT_TEMPLATE_DEF(SceneTransformComponent)

   TYPER_BEGIN(TestCustomUIComponent)
      TYPER_FIELD(integer)
      TYPER_FIELD(floating)
      TYPER_FIELD(float3)
   TYPER_END(TestCustomUIComponent)

   COMPONENT_EXPLICIT_TEMPLATE_DEF(TestCustomUIComponent)

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

      id = GetTypeID<TestCustomUIComponent>();
      ComponentList::Get().RegisterComponent(id, [](Entity& entity) {
         if (auto* c = entity.TryGet<TestCustomUIComponent>()) {
            EditorUI<TestCustomUIComponent>(c->GetName(), *c);
         }
         });

      return 0;
   }

   static int __unused_value = RegisterComponents();

}
