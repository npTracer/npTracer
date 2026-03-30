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

struct LightRecord
{
    uint transformIndex;
    float4 color;
    float intensity;
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

[shader("vertex")]
VertexOutput vertMain(uint vid : SV_VertexID) {
    VertexOutput output;
    
    MeshRecord rec = meshRecords[pushConstants.meshIndex];
    
    // geometry
    uint index = globalIndices[rec.indexOffset + vid];
    Vertex input = globalVertices[index];
    
    // transform
    float4x4 transform = globalTransforms[rec.transformIndex];
    
    float4 worldPos = mul(transform, input.pos);
    output.worldPos = worldPos.xyz;
    
    output.worldNormal = normalize(mul((float3x3)transform, input.normal.xyz)); // TODO use inverse transpose
    
    float4 viewPos  = mul(camera.view, worldPos);
    output.pos      = mul(camera.proj, viewPos);

    output.color  = input.color.xyz;
    output.uv = input.uv;

    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput input) : SV_Target
{
    float3 N = normalize(input.worldNormal);
    float3 baseColor = input.color;
    
    float3 litColor = float3(0.0, 0.0, 0.0);
    
    LightRecord light = lights[0]; // TODO use more than one light
    float4x4 lightTransform = lightTransforms[light.transformIndex];
    
    float3 lightPos = float3(
        lightTransform[0][3],
        lightTransform[1][3],
        lightTransform[2][3]
    );
    float3 L = normalize(lightPos - input.worldPos);
    
    float NdotL = max(dot(N, L), 0.0);
    litColor += baseColor * light.color.rgb * light.intensity * NdotL;
    
    return float4(litColor, 1.0);
}