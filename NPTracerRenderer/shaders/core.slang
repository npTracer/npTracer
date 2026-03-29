static const uint MAX_RESOURCE_COUNT = 10000;

struct MeshRecord
{
    uint vbIdx;
    uint ibIdx;
};

struct Vertex
{
    float3 pos;
    float3 color;
    float2 uv;
}

struct Camera
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

struct Light
{
    float4x4 matrix;
    float3 color;
    float intensity;
};

struct VertexOutput
{
    float4 pos : SV_Position;
    float3 color : COLOR0;
    float2 uv : TEXCOORD0;
};

// set 0
[[vk::binding(0, 1)]]
ConstantBuffer<Camera> cam;

// set 1
[[vk::binding(0, 1)]]
StructuredBuffer<MeshRecord> meshRecords;

[[vk::binding(1, 1)]]
StructuredBuffer<Vertex> vertexBuffers[MAX_RESOURCE_COUNT];

[[vk::binding(2, 1)]]
StructuredBuffer<uint> indexBuffers[MAX_RESOURCE_COUNT];

// set 2
//[[vk::binding(0, 2)]]
//StructuredBuffer<Light> lights;

[shader("vertex")]
VertexOutput vertMain(uint vertexID: SV_VertexID, uint instanceID: SV_InstanceID)
{
    VertexOutput output;
    MeshRecord rec = meshRecords[instanceID];

    uint vb = NonUniformResourceIndex(rec.vbIdx);
    uint ib = NonUniformResourceIndex(rec.ibIdx);

    uint index = indexBuffers[ib][vertexID];
    Vertex vertex = vertexBuffers[vb][index];

    float4 worldPos = mul(cam.model, float4(vertex.pos, 1.0));
    float4 viewPos = mul(cam.view, worldPos);
    output.pos = mul(cam.proj, viewPos);
    
    output.color = vertex.color;
    output.uv = vertex.uv;

    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}