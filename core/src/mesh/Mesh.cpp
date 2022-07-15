#include "pch.h"
#include "Mesh.h"
#include "rend/RendRes.h"
#include "rend/Buffer.h"


namespace pbe {

   MeshGeom MeshGeomCube() {
      MeshGeom cube{ sizeof(VertexPosNormal) };

      // todo: super dirty
      cube.vertexes.resize(sizeof(VertexPosNormal) * 8);

      // auto a = cube.VertexesAs<VertexPosNormal>();
      // a =
      cube.VertexesAs<VertexPosNormal>() =
      {
         {{ -1.0f, 1.0f, -1.0f}, {0, 0, 255}, },
         {{ 1.0f, 1.0f, -1.0f}, {0, 255, 0}, },
         {{ -1.0f, -1.0f, -1.0f}, {255, 0, 0}, },
         {{ 1.0f, -1.0f, -1.0f}, {0, 255, 255}, },
         {{ -1.0f, 1.0f, 1.0f}, {0, 0, 255}, },
         {{ 1.0f, 1.0f, 1.0f}, {255, 0, 0}, },
         {{ -1.0f, -1.0f, 1.0f}, {0, 255, 0}, },
         {{ 1.0f, -1.0f, 1.0f}, {0, 255, 255}, },
      };

      cube.indexes = {
         0, 1, 2,    // side 1
         2, 1, 3,
         4, 0, 6,    // side 2
         6, 0, 2,
         7, 5, 6,    // side 3
         6, 5, 4,
         3, 1, 7,    // side 4
         7, 1, 5,
         4, 5, 0,    // side 5
         0, 5, 1,
         3, 7, 2,    // side 6
         2, 7, 6,
      };

      return cube;
   }

   Mesh Mesh::Create(MeshGeom&& geom) {
      Mesh mesh{ std::move(geom) };

      auto bufferDesc = Buffer::Desc::VertexBuffer(mesh.geom.VertexesBytes());
      mesh.vertexBuffer = Buffer::Create(bufferDesc, mesh.geom.vertexes.data());
      mesh.vertexBuffer->SetDbgName("vertex buf");

      bufferDesc = Buffer::Desc::IndexBuffer(mesh.geom.IndexesBytes());
      mesh.indexBuffer = Buffer::Create(bufferDesc, mesh.geom.indexes.data());
      mesh.indexBuffer->SetDbgName("index buf");

      return mesh;
   }

}
