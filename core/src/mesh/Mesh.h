#pragma once
#include "core/Core.h"
#include "core/Ref.h"
#include "math/Types.h"


namespace pbe {
   class Buffer;


   class MeshGeom {
   public:
      std::vector<byte> vertexes;
      std::vector<int16> indexes;

      uint nVertexByteSize = 0;

      MeshGeom() = default;
      MeshGeom(int nVertexByteSize) : nVertexByteSize(nVertexByteSize) {}

      int VertexesBytes() const { return (int)vertexes.size(); }
      int IndexesBytes() const { return IndexCount() * sizeof(uint16); }
      int VertexCount() const { return int(vertexes.size() / nVertexByteSize); }
      int IndexCount() const { return (int)indexes.size(); }

      template<typename T>
      std::vector<T>& VertexesAs() {
         return *(std::vector<T>*) & vertexes;
      }
   };

   MeshGeom MeshGeomCube();


   class Mesh {
   public:
      Mesh() = default;
      Mesh(MeshGeom&& geom) : geom(std::move(geom)) {}

      Ref<Buffer> vertexBuffer;
      Ref<Buffer> indexBuffer;

      MeshGeom geom;

      static Mesh Create(MeshGeom&& geom);
   };

}
