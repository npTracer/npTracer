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
    float4x4 view;
    float4x4 proj;
    float4x4 invView;
    float4x4 invProj;
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

struct Payload
{
    float3 color;
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
    uint2 pixel = DispatchRaysIndex().xy; // pixel responsible for dispatch
    uint2 dims = DispatchRaysDimensions().xy; // image dimensions in pixels
    
    float2 uv =  (float2(pixel) + 0.5) / float2(dims); // get pixel center
    
    float4 origin = mul(camera.invView, float4(0.0, 0.0, 0.0, 1.0)); // camera origin in world space
    
    float2 clipXY = uv * 2.0 - 1.0;
    float4 target = mul(camera.invProj, float4(clipXY, 1.0, 1.0)); // in homogeneous view space
    target /= target.w;
    
    float3 dirView = normalize(target.xyz);
    float3 dir = normalize(mul((float3x3)camera.invView, dirView)); // in world space
    
    float tMin = 0.001;
    float tMax = 10000.0;
    
    RayDesc ray;
    ray.Direction = dir.xyz;
    ray.Origin = origin.xyz;
    ray.TMin = tMin;
    ray.TMax = tMax;
    
    Payload payload;
    payload.color = float3(0.0, 0.0, 0.0);
    
    TraceRay(
        scene,
        RAY_FLAG_NONE,
        0xFF,
        0, // hit group index
        0, // hit group multiplier
        0, // miss shader index
        ray,
        payload);
    
    resultImage[pixel] = float4(payload.color.bgr, 1.0);
}

[shader("miss")]
void missMain(inout Payload payload)
{
    payload.color = float3(0.0, 0.0, 0.0);
}

[shader("closesthit")]
void hitMain(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // instance data
    uint instanceIdx = InstanceID();
    MeshRecord rec = meshRecords[instanceIdx];
    
    float4x4 transform = globalTransforms[rec.transformIndex];
    MaterialRecord material = materials[rec.materialIndex];

    // TODO: register file doesn't like this
    uint primIdx = PrimitiveIndex();
    
    uint i0 = globalIndices[rec.indexOffset + primIdx * 3 + 0];
    uint i1 = globalIndices[rec.indexOffset + primIdx * 3 + 1];
    uint i2 = globalIndices[rec.indexOffset + primIdx * 3 + 2];
    
    Vertex v0 = globalVertices[i0];
    Vertex v1 = globalVertices[i1];
    Vertex v2 = globalVertices[i2];
    
    float b0 = 1.0 - attr.barycentrics.x - attr.barycentrics.y;
    float b1 = attr.barycentrics.x;
    float b2 = attr.barycentrics.y;
    
    float4 pos = v0.pos * b0 + v1.pos * b1 + v2.pos * b2;
    float4 normal = v0.normal * b0 + v1.normal * b1 + v2.normal * b2;
    float3 worldNormal = normalize(mul((float3x3)transform, normal.xyz));
    float4 color = v0.color * b0 + v1.color * b1 + v2.color * b2;
    float2 uv = v0.uv * b0 + v1.uv * b1 + v2.uv * b2;
   
        
    // texture
    Sampler2D texture = textures[material.textureIndex];
    float4 texel = texture.SampleLevel(uv, 0.0);
    
    float3 N = normalize(worldNormal);
    float3 baseColor = material.diffuse.rgb;
    
    if (material.textureIndex != 0xFFFFFFFF)
    {
        Sampler2D texture = textures[material.textureIndex];
        float4 texel = texture.SampleLevel(uv, 0.0);
        baseColor *= texel.rgb;
    }

    payload.color = baseColor;
}