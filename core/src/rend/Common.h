#pragma once

#include <string_view>
#include <wrl/client.h>

struct ID3D11DeviceChild;

template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

namespace pbe {

   void SetDbgName(ID3D11DeviceChild* obj, std::string_view name);

}
