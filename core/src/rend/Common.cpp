#include "pch.h"
#include "Common.h"
#include "Device.h"

namespace pbe {

   void SetDbgName(ID3D11DeviceChild* obj, std::string_view name) {
      if (!name.empty()) {
         obj->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)name.size(), name.data());
      }
   }

}
