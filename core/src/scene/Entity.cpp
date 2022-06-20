#include "Entity.h"
#include "typer/Typer.h"

Entity::Entity(entt::entity id, Scene* scene) :id(id), scene(scene) {
}


TYPER_BEGIN(SceneTransformComponent)
TYPER_FIELD(position)
TYPER_FIELD(rotation)
TYPER_FIELD(x)
TYPER_FIELD(y)
TYPER_FIELD(z)
TYPER_END(SceneTransformComponent)
