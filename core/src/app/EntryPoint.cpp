#include "pch.h"
#include "EntryPoint.h"


int pbeMain(pbe::Application* sApplication, int nArgs, char** args) {

   sApplication->OnInit();
   sApplication->Run();
   sApplication->OnTerm();
   delete sApplication;

   return 0;
}
