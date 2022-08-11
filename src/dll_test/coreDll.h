#ifdef CORE_API_EXPORT
   #define CORE_API  __declspec(dllexport)
#else
   #define CORE_API  __declspec(dllimport)
#endif

class Engine2 {
   void Func();
};

class CORE_API Engine {
public:
   Engine();
   ~Engine();

   void hello();
   static void helloStatic();

   virtual void virtualFunc();

   int val = 17;

   Engine2 e2;
};

namespace core {
   CORE_API int namespaceFunc();
}

extern "C" CORE_API int coreFunc();
