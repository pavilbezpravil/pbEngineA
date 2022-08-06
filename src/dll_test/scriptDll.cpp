#include "scriptDll.h"
#include "coreDll.h"

SCRIPT_API int scriptFunc() {
   return coreFunc();
}
