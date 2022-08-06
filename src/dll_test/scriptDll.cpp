#include "scriptDll.h"
#include "coreDll.h"

SCRIPT_API int scriptFunc() {
   Engine e;
   e.hello();

   return coreFunc();
}
