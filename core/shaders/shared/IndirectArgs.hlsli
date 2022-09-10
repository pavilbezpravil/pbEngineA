#ifndef INDIRECT_ARGS
#define INDIRECT_ARGS

struct DrawIndexedArgs {
   uint vertCount;
   uint instCount;
   uint startVert;
   uint startInstLocation;
};

struct DrawIndexedInstancedArgs {
   uint indexCount;
   uint instCount;
   uint indexStart;
   uint startVert;
   uint startInstLocation;
};

struct DispatchArgs {
   uint groupX;
   uint groupY;
   uint groupZ;
};

#endif // INDIRECT_ARGS
