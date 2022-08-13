struct SRTObject {
    float3 position;
    float _wef32;

    float3 albedo;
    float _sdfsde;
    
    float3 halfSize;
    int geomType;
};

struct SRTConstants {
    int2 rtSize;
    int rayDepth;
    int nObjects;

    int nRays;
    float random01;
    float2 _wef32;
};