#include "coreDll.h"

#include <iostream>

using namespace std;

Engine::Engine() {}

Engine::~Engine() {}

void Engine::hello() {
   cout << "hello" << endl;
}

void Engine::helloStatic() {
   cout << "helloStatic" << endl;
}

void Engine::virtualFunc() {
   cout << "virtual" << endl;
}

namespace core {
   int namespaceFunc() {
      cout << "namespaceFunc" << endl;
      return 57;
   }
}

CORE_API int coreFunc() {
   cout << "coreFunc" << endl;
   return 42;
}
