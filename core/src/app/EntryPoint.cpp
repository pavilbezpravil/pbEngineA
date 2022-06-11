#include "Application.h"

extern Application* CreateApplication();


int main(int, char**) {

   Application* sApplication = CreateApplication();
   sApplication->OnInit();
   sApplication->Run();
   sApplication->OnTerm();
   delete sApplication;

   return 0;
}
