#pragma once

#include "Common.h"

#include "Device.h"

namespace pbe {

   void SetDbgName(ID3D11DeviceChild* obj, std::string_view name) {
      obj->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)name.size(), name.data());
   }

}
