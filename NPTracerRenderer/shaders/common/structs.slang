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
    float2 metallic;
    
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
    uint terminate;
    
    float2 mr; // x: metallic, y: roughness
    float specular;
};

struct ShadowPayload
{
    bool hit;
};

// an intersection along the path of a ray
struct Intersection
{
    Payload payload; // represents intersection parameters (pos, normal, albedo, etc)
    float3 wo; // represents outgoing direction from intersection
    float3 weight; // represents weight of intersection to path (f * cos / pdf) for sampled next direction
    uint validNext; // whether a continuation ray was successfully sampled
}

struct BRDFSample
{
    float3 wi;
    float3 weight; // f * NoL / pdf
    uint valid; // whether a continuation ray wi was successfully sampled
};