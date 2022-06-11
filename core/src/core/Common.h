#pragma once

#define SAFE_DELETE(x) if (x) { \
                          delete x; \
                       }


#define NON_COPYABLE(Type) \
   Type(Type&) = delete; \
   Type(Type&&) = delete; \
   Type& operator=(Type&) = delete; \
   Type& operator=(Type&&) = delete;
