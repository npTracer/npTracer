[require(spirv_1_5)]
[require(SPV_EXT_physical_storage_buffer)]

struct MeshRecord
{
    uint vertexOffset;
    uint indexOffset;
    uint indexCount;
    uint vertexCount;
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

struct PushConstants
{
    uint meshIndex;
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
ConstantBuffer<CameraRecord> camera;

static float2 positions[3] = float2[](
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5)
);

static float3 colors[3] = float3[](
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
);

[shader("vertex")]
VertexOutput vertMain(uint vid : SV_VertexID) {
    VertexOutput output;
    
    MeshRecord meshRecord = meshRecords[pushConstants.meshIndex];
    
    if (vid >= meshRecord.indexCount)
    {
        output.pos = float4(1.0, 0.0, 0.0, 1.0);
        output.color = float3(1.0, 0.0, 1.0);
        return output;
    }
    
    uint index = globalIndices[meshRecord.indexOffset + vid];
    
    if (index < meshRecord.vertexOffset || index >= (meshRecord.vertexOffset + meshRecord.vertexCount))
    {
        output.pos = float4(1.0, 0.0, 0.0, 1.0);
        output.color = float3(1.0, 1.0, 0.0);
        return output;
    }
    
    Vertex input = globalVertices[index];
    
    float4 worldPos = mul(camera.model, input.pos);
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