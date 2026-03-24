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

struct VertexOutput
{
    float4 pos : SV_Position;
    float3 color : COLOR0;
    float2 uv : TEXCOORD0;
};

// set 0
[[vk::binding(0, 0)]]
StructuredBuffer<MeshRecord> meshRecords;

[[vk::binding(1, 0)]]
StructuredBuffer<Vertex> vertexBuffers[];

[[vk::binding(2, 0)]]
StructuredBuffer<uint> indexBuffers[];

// set 1
[[vk::binding(0, 1)]]
ConstantBuffer<Camera> cam;

[shader("vertex")]
VertexOutput vertMain(uint vertexID: SV_VertexID, uint instanceID: SV_InstanceID)
{
    VertexOutput output;
    MeshRecord rec = meshRecords[instanceID];

    uint index = indexBuffers[rec.ibIdx][vertexID];
    Vertex vertex = vertexBuffers[rec.vbIdx][index];

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