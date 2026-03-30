[require(spirv_1_5)]
[require(SPV_EXT_physical_storage_buffer)]

struct MeshRecord
{
    uint vertexOffset;
    uint indexOffset;
    uint indexCount;
    uint vertexCount;
    
    uint transformIndex;
};

struct CameraRecord
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
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
    float3 normal : NORMAL;
    float3 color : COLOR0;
    float2 uv : TEXCOORD0;
};

[[vk::binding(0, 0)]]
StructuredBuffer<MeshRecord> meshRecords;

[[vk::binding(1, 0)]]
StructuredBuffer<Vertex> globalVertices;

[[vk::binding(2, 0)]]
StructuredBuffer<uint> globalIndices;

[[vk::binding(3, 0)]]
StructuredBuffer<float4x4> globalTransforms;

[[vk::binding(0, 1)]]
ConstantBuffer<CameraRecord> camera;

[shader("vertex")]
VertexOutput vertMain(uint vid : SV_VertexID, uint iid : SV_InstanceID) {
    VertexOutput output;
    
    MeshRecord rec = meshRecords[iid];
    
    // geometry
    uint index = globalIndices[rec.indexOffset + vid];
    Vertex input = globalVertices[index];
    
    // transform
    float4x4 transform = globalTransforms[rec.transformIndex];
    
    float4 worldPos = mul(transform, input.pos);
    float4 viewPos  = mul(camera.view, worldPos);
    output.pos      = mul(camera.proj, viewPos);

    output.color  = input.color.xyz;

    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}