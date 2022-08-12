#include "pch.h"
#include "LayerStack.h"

namespace pbe {

   LayerStack::LayerStack() {
      // layers.reserve(2);
      layerInsert = layers.begin();
   }

   LayerStack::~LayerStack() {
      Clear();
   }

   void LayerStack::PushLayer(Layer* layer) {
      layerInsert = layers.emplace(layerInsert, layer);
   }

   void LayerStack::PushOverlay(Layer* overlay) {
      layers.emplace_back(overlay);
      if (!layerInsert._Ptr) { // todo:
         layerInsert = layers.begin();
      }
   }

   void LayerStack::PopLayer(Layer* layer) {
      auto it = std::find(layers.begin(), layers.end(), layer);
      if (it != layers.end()) {
         layers.erase(it);
         --layerInsert;
      }

   }

   void LayerStack::PopOverlay(Layer* overlay) {
      auto it = std::find(layers.begin(), layers.end(), overlay);
      if (it != layers.end()) {
         layers.erase(it);
      }
   }

   void LayerStack::Clear() {
      for (Layer* layer : layers) {
         delete layer;
      }

      layers.clear();
   }

}
