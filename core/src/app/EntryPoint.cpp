#include "pch.h"
#include "Application.h"

namespace pbe {
   extern Application* CreateApplication();
}


int main(int, char**) {

   pbe::Application* sApplication = pbe::CreateApplication();
   sApplication->OnInit();
   sApplication->Run();
   sApplication->OnTerm();
   delete sApplication;

   return 0;
}
