struct MeshRecord
{
    uint32_t vertexOffset;
    uint32_t indexOffset;
    uint32_t indexCount;
    uint32_t vertexCount;
    uint32_t transformIndex;
    uint32_t materialIndex;
};

struct CameraRecord
{
    float4x4 view;
    float4x4 proj;
    float4x4 invView;
    float4x4 invProj;
};

struct LightRecord
{
    float4x4 transform;
    float4 color;
    float intensity;
};

struct MaterialRecord
{
    float4 diffuse;
    float4 ambient;
    float4 specular;
    float4 emissive;
    
    uint32_t diffuseTextureIndex;
    uint32_t normalTextureIndex;
    uint32_t metallicTextureIndex;
};

struct Vertex
{
    float4 pos;
    float4 normal;
    float4 tangent;
    float4 color;
    float2 uv;
    float2 pad0;
};

struct VertexOutput
{
    float4 pos : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float3 color : COLOR0;
    float2 uv : TEXCOORD2;
};

struct PushConstants
{
    uint32_t meshIndex;
    uint32_t numLights;
    uint32_t frameIndex;
};

struct Payload
{
    bool hit;
    float3 albedo;
    float3 normal;
    float3 hitPos;
    float4 emission;
    uint style;
};

struct ShadowPayload
{
    bool hit;
};