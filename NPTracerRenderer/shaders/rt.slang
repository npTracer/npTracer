[require(spirv_1_5)]
[require(SPV_EXT_physical_storage_buffer)]

struct MeshRecord
{
    uint vertexOffset;
    uint indexOffset;
    uint indexCount;
    uint vertexCount;
    
    uint transformIndex;
    uint materialIndex;
};

struct CameraRecord
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

struct LightRecord
{
    uint transformIndex;
    float4 color;
    float intensity;
};

struct MaterialRecord
{
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float4 emissive;
    uint textureIndex;
};

struct Vertex
{
    float4 pos;
    float4 normal;
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
    uint meshIndex;
    uint numLights;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pushConstants;

[[vk::binding(0, 0)]]
StructuredBuffer<MeshRecord> meshRecords;

[[vk::binding(1, 0)]]
StructuredBuffer<Vertex> globalVertices;

[[vk::binding(2, 0)]]
StructuredBuffer<uint> globalIndices;

[[vk::binding(0, 1)]]
StructuredBuffer<float4x4> globalTransforms;

[[vk::binding(1, 1)]]
StructuredBuffer<float4x4> lightTransforms;

[[vk::binding(0, 2)]]
ConstantBuffer<CameraRecord> camera;

[[vk::binding(1, 2)]]
StructuredBuffer<LightRecord> lights;

[[vk::binding(0, 3)]]
StructuredBuffer<MaterialRecord> materials;

[[vk::binding(1, 3)]]
Sampler2D textures[];

[[vk::binding(0, 4)]]
RaytracingAccelerationStructure scene;

[[vk::binding(1, 4)]]
RWTexture2D<float4> resultImage;

[shader("raygeneration")]
void rgenMain()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dims = DispatchRaysDimensions().xy;
    
    float2 uv = (float2(pixel) + 0.5) / float2(dims);
    
    resultImage[pixel] = float4(uv, 0.0, 1.0);
}

[shader("miss")]
void missMain()
{

}

[shader("closesthit")]
void hitMain()
{

}