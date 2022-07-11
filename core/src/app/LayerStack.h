#pragma once

#include "Layer.h"

#include <vector>

namespace pbe {

   class LayerStack {
   public:
      LayerStack();
      ~LayerStack();

      void PushLayer(Layer* layer);
      void PushOverlay(Layer* overlay);
      void PopLayer(Layer* layer);
      void PopOverlay(Layer* overlay);

      void Clear();

      using Layers = std::vector<Layer*>;

      Layers::iterator begin() { return layers.begin(); }
      Layers::iterator end() { return layers.end(); }
   private:
      Layers layers;
      Layers::iterator layerInsert;
   };

}
