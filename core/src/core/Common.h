#pragma once

#define SAFE_DELETE(x) if (x) { \
                          delete x; \
                          x = nullptr; \
                       }

#define SAFE_RELEASE(x) if (x) { \
                           x->Release(); \
                           x = nullptr; \
                        }


#define NON_COPYABLE(Type) \
   Type(Type&) = delete; \
   Type(Type&&) = delete; \
   Type& operator=(Type&) = delete; \
   Type& operator=(Type&&) = delete;
