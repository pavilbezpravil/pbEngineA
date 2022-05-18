#include "dll_example.h"
#include <iostream>

using namespace std;

DllExample::DllExample() {}

DllExample::~DllExample() {}

void DllExample::hello() {
   cout << "Hello World of DLL" << endl;
}

void DllExample::helloStatic() {
   cout << "Hello World of DLL static" << endl;
}

void dllFunc() {
   cout << "Hello World of dllFunc" << endl;
}
