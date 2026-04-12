[require(spirv_1_5)]
[require(SPV_EXT_physical_storage_buffer)]
[require(spvShaderNonUniformEXT)]

/*
NPTracer coordinate system conventions:
Right-handedness - up=+Y, forward=-Z
Flipped UVs - origin (0,0) at top-left corner, (1,1) at bottom-right corner
*/

// TODO: move all shared structs and bindings to a common shader module

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

[[vk::constant_id(0)]] const uint32_t kFlipUVY;

[[vk::push_constant]]
ConstantBuffer<PushConstants> pushConstants;

[[vk::binding(0, 0)]]
StructuredBuffer<MeshRecord> meshRecords;

[[vk::binding(1, 0)]]
StructuredBuffer<Vertex> globalVertices;

[[vk::binding(2, 0)]]
StructuredBuffer<uint32_t> globalIndices;

[[vk::binding(3, 0)]]
StructuredBuffer<float4x4> globalTransforms;

[[vk::binding(0, 1)]]
StructuredBuffer<LightRecord> lights;

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
    // map vulkan top-left UV origin convention to NDC space (-1, -1)
    float clipX = uv.x * 2.0 - 1.0; // i.e. 0 to -1, 1 to 1
    float clipY = uv.y * -2.0 + 1.0; // i.e. 0 to 1, 1 to -1
    float4 target = mul(camera.invProj, float4(clipX, clipY, 1.0, 1.0)); // in homogeneous view space
    target /= target.w;
    
    float3 dirView = normalize(target.xyz);
    float3 dir = normalize(mul((float3x3)camera.invView, dirView)); // in world space
    
    return dir;
}

// https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
uint32_t pcg_hash(uint32_t x)
{
    x = x * 747796405u + 2891336453u;
    uint32_t r = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
    return (r >> 22u) ^ r;
}

float rand(inout uint32_t state)
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

    float3 Lo = float3(0.0);

    for (uint32_t i = 0; i < pushConstants.numLights; i++)
    {
        LightRecord light = lights[i];
        float4x4 lightTransform = light.transform;

        float3 lightPos = float3(
            lightTransform[0][3],
            lightTransform[1][3],
            lightTransform[2][3]
        );

        float3 L = lightPos - P;
        float3 LNorm = normalize(L);
        float dist = length(lightPos - P);

        float NdotL = max(dot(N, LNorm), 0.0);
        if (NdotL <= 0.0) continue;

        RayDesc shadowRay;
        shadowRay.Origin = P + N * 0.001;
        shadowRay.Direction = LNorm;
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

        float distSq = max(dot(L, L), 1e-4);
        
        float attenuation = 1.0 / max(distSq, 1e-4);
        float3 Li = light.color.rgb * light.intensity * attenuation;
        
        float3 f = albedo / 3.14159265; // lambertian

        Lo += f * Li * NdotL;
    }

    return Lo;
}

float luminance(float3 samples)
{
    return dot(samples, float3(0.2126, 0.7152, 0.0722));
}

float3 stylizeGreyScale(float3 samples)
{
    let lum = luminance(samples);
    return float3(lum, lum, lum);
}

[shader("raygeneration")]
void rgenMain()
{
    uint32_t2 pixel = DispatchRaysIndex().xy; // pixel responsible for dispatch
    uint32_t2 dims = DispatchRaysDimensions().xy; // image dimensions in pixels
    
    float3 cameraOrigin = mul(camera.invView, float4(0.0, 0.0, 0.0, 1.0)).xyz; // camera origin in world space
    
    float tMin = 0.001;
    float tMax = 10000.0;
    
    uint32_t samples = 5;
    uint32_t bounces = 3;
    
    float3 finalColor = float3(0.0);
    
    // random init
    uint32_t baseSeed = pixel.x + pixel.y * dims.x;
    
    for (uint32_t sample = 0; sample < samples; sample++)
    {
        uint32_t seed = pcg_hash(
            baseSeed
            ^ (sample * 0x9E3779B9u)
            ^ (pushConstants.frameIndex * 0x85EBCA6Bu)
        );
    
        // generate a ray for this pixel
        float2 jitter = float2(rand(seed), rand(seed));
        float2 uv = (float2(pixel) + jitter) / dims;
        uv.y = lerp(uv.y, 1.0 - uv.y, float(kFlipUVY)); // use lerp to flip coord system Y w/o a boolean check
        
        float3 origin = cameraOrigin;
        float3 wi = generateCameraRayDir(uv);
        
        float3 radiance = float3(0.0); // light gathered along the path
        float3 throughput = float3(1.0); // energy that survives along path
        
        bool stylize = false;
        
        // shoot ray (bounceable) into the scene
        for (uint32_t bounce = 0; bounce < bounces; bounce++)
        {
            // given the new ray direction and origin shoot off in direction
            Payload payload;
            
            RayDesc ray;
            ray.Direction = wi.xyz;
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
            
            // hit a surface
            if (!payload.hit)
            {
                radiance += throughput * float3(0.0, 0.0, 0.0);
                break;
            }
            
            float3 currRadiance = float3(0.0, 0.0, 0.0);
            // direct lighting step
            float3 Ld = computeDirectLighting(payload, -wi);
            currRadiance += Ld * throughput;
            
            // check surface emission
            float3 Le = payload.emission.rgb * payload.emission.w;
            currRadiance += Le * throughput;
            
            // sample wi from pdf
            float3 N = normalize(payload.normal);
            float2 xi = float2(rand(seed), rand(seed));
            float3 localDir = cosineSampleHemisphere(xi);
            float3 dir = transformWorld(localDir, N);
            
            // update throughput
            float3 f = payload.albedo; // lambertian
            throughput *= f; 
                        
            if (payload.style == 1)
            {
                currRadiance = stylizeGreyScale(currRadiance);
            }

            radiance += currRadiance;
            
            // continue path
            wi = dir;
            origin = payload.hitPos + wi * 0.001f;
        }
        
        finalColor += radiance;
    }
    
    finalColor /= samples;
    float3 curr = finalColor;

    // accumulate and tonemap

    if (pushConstants.frameIndex == 0)
    {
        accumulationImage[pixel] = float4(curr.rgb, 1.0); // if first frame store sample directly
    }
    else
    {
        float3 prev = accumulationImage[pixel].rgb;
        float n = float(pushConstants.frameIndex);
        curr = (prev * n + curr) / (n + 1.0); // incorporate new sample into running summation (the n + 1 sample)
        accumulationImage[pixel] = float4(curr.rgb, 1.0); // store new running average
    }
    
    // tonemap output
    float3 display = curr;
    float luma = luminance(display);
    display = display * (luma / (luma + 1.0)) / max(luma, 1e-5); // = display / (luma + 1)
    
    resultImage[pixel] = float4(display.rgb, 1.0);
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
    uint32_t instanceIdx = InstanceID();
    MeshRecord rec = meshRecords[instanceIdx];
    
    float4x4 transform = globalTransforms[rec.transformIndex];
    MaterialRecord material = materials[rec.materialIndex];

    // TODO: register file doesn't like this
    uint32_t primIdx = PrimitiveIndex();
    
    uint32_t i0 = globalIndices[rec.indexOffset + primIdx * 3 + 0];
    uint32_t i1 = globalIndices[rec.indexOffset + primIdx * 3 + 1];
    uint32_t i2 = globalIndices[rec.indexOffset + primIdx * 3 + 2];
    
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
    
    if (material.diffuseTextureIndex != 0xFFFFFFFF)
    {
        Sampler2D texture = textures[material.diffuseTextureIndex];
        float4 texel = texture.SampleLevel(uv, 0.0);
        baseColor *= texel.rgb;
        
        // temp stylize textured objects
        //payload.style = 1;
    }
    
    payload.hit = true;
    payload.albedo = baseColor;
    payload.emission = material.emissive;
    payload.normal = worldNormal;
    payload.hitPos = worldPos;
}

[shader("closesthit")]
void shadowhitMain(inout ShadowPayload payload)
{
    payload.hit = true;
}