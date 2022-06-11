#pragma once

#include <string>

#include "core/Common.h"

struct Event;


class Layer {
   NON_COPYABLE(Layer);
public:
   Layer(const std::string& name = "Layer");
   virtual ~Layer() = default;

   virtual void OnAttach() {
   }

   virtual void OnDetach() {
   }

   virtual void OnUpdate(float dt) {
   }

   virtual void OnImGuiRender() {
   }

   virtual void OnEvent(Event& event) {
   }

   const std::string& GetName() const { return name; }
protected:
   std::string name;
};
