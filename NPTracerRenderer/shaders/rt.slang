[require(spirv_1_5)]
[require(SPV_EXT_physical_storage_buffer)]

// TODO: move all shared structs and bindings to a common shader module

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
    uint frameIndex;
    uint flipNDCY;
};

struct Payload
{
    bool hit;
    float3 albedo;
    float3 normal;
    float3 hitPos;
};

struct ShadowPayload
{
    bool hit;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pushConstants;

[[vk::binding(0, 0)]]
StructuredBuffer<MeshRecord> meshRecords;

[[vk::binding(1, 0)]]
StructuredBuffer<Vertex> globalVertices;

[[vk::binding(2, 0)]]
StructuredBuffer<uint> globalIndices;

[[vk::binding(3, 0)]]
StructuredBuffer<float4x4> globalTransforms;

[[vk::binding(0, 1)]]
StructuredBuffer<LightRecord> lights;

[[vk::binding(1, 1)]]
StructuredBuffer<float4x4> lightTransforms;

[[vk::binding(0, 2)]]
ConstantBuffer<CameraRecord> camera;

[[vk::binding(0, 3)]]
StructuredBuffer<MaterialRecord> materials;

[[vk::binding(1, 3)]]
Sampler2D textures[];

[[vk::binding(0, 4)]]
RaytracingAccelerationStructure scene;

[[vk::binding(1, 4)]]
RWTexture2D<float4> resultImage;

[[vk::binding(2, 4)]]
RWTexture2D<float4> accumulationImage;

float3 generateCameraRayDir(float2 uv)
{
    float2 clipXY = uv * 2.0 - 1.0;
    float4 target = mul(camera.invProj, float4(clipXY, 1.0, 1.0)); // in homogeneous view space
    target /= target.w;
    
    float3 dirView = normalize(target.xyz);
    float3 dir = normalize(mul((float3x3)camera.invView, dirView)); // in world space
    
    return dir;
}

// https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
uint pcg_hash(uint x)
{
    x = x * 747796405u + 2891336453u;
    uint r = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
    return (r >> 22u) ^ r;
}

float rand(inout uint state)
{
    state = state * 1664525u + 1013904223u;
    return float(state) / 4294967296.0;
}

float3 cosineSampleHemisphere(float2 xi)
{
    float r = sqrt(xi.x);
    float theta = 2.0f * 3.14159265 * xi.y;
    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(max(0.0, 1.0 - xi.x));
    return float3(x, y, z);
}

float3 transformWorld(float3 localDir, float3 N)
{
    float3 up = abs(N.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
    return normalize(localDir.x * tangent + localDir.y * bitangent + localDir.z * N);
}

float3 computeDirectLighting(Payload payload, float3 wo)
{
    float3 N = normalize(payload.normal);
    float3 P = payload.hitPos;
    float3 albedo = payload.albedo;

    float3 Ld = float3(0.0);

    for (uint i = 0; i < pushConstants.numLights; i++)
    {
        LightRecord light = lights[i];
        float4x4 lightTransform = lightTransforms[light.transformIndex];

        float3 lightPos = float3(
            lightTransform[0][3],
            lightTransform[1][3],
            lightTransform[2][3]
        );

        float3 L = normalize(lightPos - P);
        float dist = length(lightPos - P);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL <= 0.0) continue;

        RayDesc shadowRay;
        shadowRay.Origin = P + N * 0.001;
        shadowRay.Direction = L;
        shadowRay.TMin = 0.001;
        shadowRay.TMax = dist - 0.001;

        ShadowPayload shadowPayload;
        shadowPayload.hit = false;

        TraceRay(
            scene,
            RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
            0xFF,
            1,
            0,
            1,
            shadowRay,
            shadowPayload
        );

        if (shadowPayload.hit) continue;

        float3 toLight = lightPos - P;
        float distSq = max(dot(toLight, toLight), 1e-4);
        
        float3 Li = light.color.rgb * light.intensity / distSq;
        
        float3 f = albedo / 3.14159265;

        Ld += f * Li * NdotL;
    }

    return Ld;
}


[shader("raygeneration")]
void rgenMain()
{
    uint2 pixel = DispatchRaysIndex().xy; // pixel responsible for dispatch
    uint2 dims = DispatchRaysDimensions().xy; // image dimensions in pixels
    
    float3 cameraOrigin = mul(camera.invView, float4(0.0, 0.0, 0.0, 1.0)).xyz; // camera origin in world space
    
    float tMin = 0.001;
    float tMax = 10000.0;
    
    uint samples = 5;
    uint bounces = 3;
    
    float3 finalColor = float3(0.0);
    
    // random init
    uint baseSeed = pixel.x + pixel.y * dims.x;
    
    for (uint sample = 0; sample < samples; sample++)
    {
        uint seed = pcg_hash(
            baseSeed
            ^ (sample * 0x9E3779B9u)
            ^ (pushConstants.frameIndex * 0x85EBCA6Bu)
        );
    
        // generate a ray for this pixel
        float2 jitter = float2(rand(seed), rand(seed));
        float2 uv = (float2(pixel) + jitter) / dims;
        uv.y = lerp(uv.y, 1.0 - uv.y, float(pushConstants.flipNDCY)); // use lerp to flip coord system Y w/o a boolean check
        float3 origin = cameraOrigin;
        float3 dir = generateCameraRayDir(uv);
        
        float3 throughput = float3(1.0); // energy that survives along path
        float3 radiance = float3(0.0); // light gathered along the path
        
        // shoot ray (bounceable) into the scene
        for (uint bounce = 0; bounce < bounces; bounce++)
        {
            // given the new ray direction and origin shoot off in direction
            Payload payload;
            
            RayDesc ray;
            ray.Direction = dir.xyz;
            ray.Origin = origin.xyz;
            ray.TMin = tMin;
            ray.TMax = tMax;
            
            TraceRay(
                 scene,
                 RAY_FLAG_NONE,
                 0xFF,
                 0, // hit group index
                 0, // hit group multiplier
                 0, // miss shader index
                 ray,
                 payload);
            
            if (!payload.hit)
            {
                radiance += throughput * float3(0.0, 0.0, 0.0);
                break;
            }
            
            // accumuluate
            float3 Ld = computeDirectLighting(payload, -dir);
            radiance += throughput * Ld;
            throughput *= payload.albedo;
            
            // sample path 
            float3 N = normalize(payload.normal);
            float2 xi = float2(rand(seed), rand(seed));
            float3 localDir = cosineSampleHemisphere(xi);
            float3 newDir = transformWorld(localDir, N);
            
            // continue path
            dir = newDir;
            origin = payload.hitPos + dir * 0.001f;
        }
        
        finalColor += radiance;
    }
    
    finalColor /= samples;
   
    float3 curr = finalColor;
    
    if (pushConstants.frameIndex == 0)
    {
        accumulationImage[pixel] = float4(curr.rgb, 1.0);
        resultImage[pixel] = float4(curr.rgb, 1.0);
        return;
    }
    
    float3 prev = accumulationImage[pixel].rgb;
    float n = float(pushConstants.frameIndex);
    float3 blended = (prev * n + curr) / (n + 1.0);
    
    accumulationImage[pixel] = float4(blended.rgb, 1.0);
    resultImage[pixel] = float4(blended.rgb, 1.0);
}

[shader("miss")]
void missMain(inout Payload payload)
{
    payload.hit = false;
}

[shader("miss")]
void shadowmissMain(inout ShadowPayload payload)
{
    payload.hit = false;
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
    float4 color = v0.color * b0 + v1.color * b1 + v2.color * b2;
    float2 uv = v0.uv * b0 + v1.uv * b1 + v2.uv * b2;
   
    float3 worldPos = mul(transform, pos).xyz;
    float3 worldNormal = normalize(mul((float3x3)transform, normal.xyz)); // TODO: inverse transpose
    
    float3 N = normalize(worldNormal);
    float3 baseColor = material.diffuse.rgb;
    
    if (material.textureIndex != 0xFFFFFFFF)
    {
        Sampler2D texture = textures[material.textureIndex];
        float4 texel = texture.SampleLevel(uv, 0.0);
        baseColor *= texel.rgb;
    }

    payload.hit = true;
    payload.albedo = baseColor;
    payload.normal = worldNormal;
    payload.hitPos = worldPos;
}

[shader("closesthit")]
void shadowhitMain(inout ShadowPayload payload)
{
    payload.hit = true;
}