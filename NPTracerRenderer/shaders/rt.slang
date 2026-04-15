[require(spirv_1_5)]
[require(SPV_EXT_physical_storage_buffer)]
[require(spvShaderNonUniformEXT)]

/*
NPTracer coordinate system conventions:
Right-handedness - up=+Y, forward=-Z
Flipped UVs - origin (0,0) at top-left corner, (1,1) at bottom-right corner
*/

import structs;
import utils;
import stylizations;
import interactions;

static const uint32_t kSampleCount = 5;
static const uint32_t kMaxBounces = 5;

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

// convert uv coorindate (within pixel) to world space direction
float3 generateCameraRayDir(float2 uv)
{
    // map vulkan top-left UV origin convention to NDC space (-1, -1)
    float clipX = uv.x * 2.0 - 1.0; // i.e. 0 to -1, 1 to 1
    float clipY = uv.y * -2.0 + 1.0; // i.e. 0 to 1, 1 to -1
    float4 target = mul(camera.invProj, float4(clipX, clipY, 1.0, 1.0)); // in homogeneous view space
    target /= target.w;
    
    // convert from view to world space
    float3 dirView = normalize(target.xyz);
    float3 dir = normalize(mul((float3x3)camera.invView, dirView)); // in world space
    
    return dir;
}

// determines radiance going in outgoing direction wo from light sources
float3 computeDirectLighting(Payload payload, float3 wo)
{
    // use the same intersection values from intersection (normal, pos, color, etc)
    float3 N = normalize(payload.normal);
    float3 P = payload.hitPos;
    float3 albedo = payload.albedo;

    float3 Lo = float3(0.0); // keeps track of outgoing radiance over all point lights

    // for each point light in the scene
    for (uint32_t i = 0; i < pushConstants.numLights; i++)
    {
        // recover light world position
        LightRecord light = lights[i];
        float4x4 lightTransform = light.transform;
        float3 lightPos = float3(
            lightTransform[0][3],
            lightTransform[1][3],
            lightTransform[2][3]
        );

        // recover vector from position to light
        float3 L = lightPos - P;
        float3 LNorm = normalize(L);
        float dist = length(lightPos - P);

        // recover dot product between light and normal of intersection
        float NdotL = max(dot(N, LNorm), 0.0);
        if (NdotL <= 0.0) continue;

        // shoot ray query from position towards light to see if position is obscured
        RayDesc shadowRay;
        shadowRay.Origin = P + N * 0.001;
        shadowRay.Direction = LNorm;
        shadowRay.TMin = 0.001;
        shadowRay.TMax = dist - 0.001;

        ShadowPayload shadowPayload;
        shadowPayload.hit = false;

        // trace ray towards light to determine obscurity
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

        // if nothing was obscured we can determine the contribution of the light to the output
        float distSq = max(dot(L, L), 1e-4); // square distance from position to light
        float attenuation = 1.0 / max(distSq, 1e-4); // linear falloff with distance
        float3 Li = light.intensity * attenuation; // pure incoming irradiance to x 
        
        // determine brdf evaluation
        float3 f = evalMicrofacetBRDF(N, wo, LNorm, payload.albedo, payload.mr, payload.specular);
        Lo += light.color.rgb * f * Li * NdotL; // add contribution to total across lights for x
    }

    return Lo;
}

[shader("raygeneration")]
void rgenMain()
{
    // initialze ray tracing params
    uint32_t2 pixel = DispatchRaysIndex().xy; // pixel responsible for dispatch
    uint32_t2 dims = DispatchRaysDimensions().xy; // image dimensions in pixels
    
    float3 cameraOrigin = mul(camera.invView, float4(0.0, 0.0, 0.0, 1.0)).xyz; // camera origin in world space
    
    // ray bounds
    float tMin = 0.001;
    float tMax = 10000.0;
    
    // sample and bounce maximums
    uint32_t samples = kSampleCount;
    uint32_t bounces = kMaxBounces;
    
    float3 finalColor = float3(0.0); // final color of the entire pixel across samples
    
    // generate seed base
    uint32_t baseSeed = pixel.x + pixel.y * dims.x;
    
    // each loop describes one evaluation of the rendering equation E[integral]
    for (uint32_t sample = 0; sample < samples; sample++)
    {
        // generate seed based on sample and frame index
        uint32_t seed = pcg_hash(
            baseSeed
            ^ (sample * 0x9E3779B9u)
            ^ (pushConstants.frameIndex * 0x85EBCA6Bu)
        );
    
        // generate a ray direction for this pixel by jittering uv within pixel
        float2 jitter = float2(rand(seed), rand(seed));
        float2 uv = (float2(pixel) + jitter) / dims;
        uv.y = lerp(uv.y, 1.0 - uv.y, float(kFlipUVY)); // use lerp to flip coord system Y w/o a boolean check
        float3 rayDir = generateCameraRayDir(uv);
        
        float3 origin = cameraOrigin; // origin is first point from which rays are shot
       
        // set up data structure for intersection data to be collected during a forward pass of a ray
        Intersection path[kMaxBounces];
        uint32_t pathLength = 0;

        // shoot ray (bounceable) into the scene and collect list of intersections
        for (uint32_t bounce = 0; bounce < bounces; bounce++)
        {
            Payload payload; // determines intersection qualities for this bounce (pos, normal, color, etc)
        
            // describe ray to be shot out with rayDir, origin, and limits (t)
            RayDesc ray;
            ray.Direction = rayDir.xyz;
            ray.Origin = origin.xyz;
            ray.TMin = tMin;
            ray.TMax = tMax;
            
            // trace the ray
            TraceRay(
                 scene,
                 RAY_FLAG_NONE,
                 0xFF,
                 0, // hit group index
                 0, // hit group multiplier
                 0, // miss shader index
                 ray,
                 payload);
            
            // if a surface has not been encountered by ray do not record intersection and return
            if (!payload.hit)
            {
                break;
            }
            
            float3 wo = -rayDir; // outgoing direction of radiance at this intersection
            float3 N = normalize(payload.normal); // intersection normal
            
            // evaluate brdf at this intersection (returns f_r and new wi ray direction)
            BRDFSample bs = sampleMicrofacetBRDF(
                N,
                wo,
                payload.albedo,
                payload.mr,
                payload.specular,
                seed
            );
            
            // record intersection data and sample data at x
            path[pathLength].payload = payload;
            path[pathLength].wo = wo;
            path[pathLength].weight = bs.weight; // (f * cos / pdf) for sampled next direction wi
            path[pathLength].validNext = bs.valid; // if valid continuation direction wi was sampled

            // if emissive light source was encountered stop tracing rays
            if (payload.terminate == true)
            {
                path[pathLength].validNext = 0;
            }         
               
            pathLength++; // path length increases
            
            // if valid continuation direction wi was not sampled stop recording and break
            if (!path[pathLength - 1].validNext)
            {
                break;
            }
            
            // continue path using new wi from brdf and hitPos
            rayDir = bs.wi;
            origin = payload.hitPos + bs.wi * 0.001f;
        }
        
        // backward fold: compute outgoing radiance going from path end back towards camera
        float3 Lo = float3(0.0); // outgoing radiance from next intersection along path
        
        for (int i = (int)pathLength - 1; i >= 0; i--) // decrement backwards
        {
            // retrieve intersection context (pos, normal, albedo, wo direction)
            Payload payload = path[i].payload;
            float3 wo = path[i].wo;
            
            // calculate lighting contributions at this intersection
            float3 Le = payload.emission.rgb * payload.emission.w;
            float3 Ld = computeDirectLighting(payload, wo);
            
            // indirect lighting step
            float3 Li = float3(0.0);
            if (path[i].validNext == 1) // if valid next direction
            {
                Li = path[i].weight * Lo; // brdf weight times previous outgoing radiance value
            }
            
            float3 L = Le + Ld + Li; // outgoing radiance from this intersection is Le + Ld + Li
            
            // apply stylization function to the outgoing radiance from this intersection
            if (payload.style == 1)
            {
                //L = stylizeGreyScale(L);
                //L = stylizeContour(L, payload.normal, wo);
                //L = stylizeStripes(L, payload.hitPos);
                //L = stylizeToon(L);
            }
            
            Lo = L; // update outgoing radiance from this point with (potentially) stylized radiance
        }
        
        finalColor += Lo; // accumulate radiance values across samples
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
    payload.terminate = false;

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
    float3 worldNormal = normalize(mul((float3x3)transform, normal.xyz));
    float3 N = normalize(worldNormal);
    
    float3 baseColor = material.diffuse.rgb;
    
    if (material.diffuseTextureIndex != 0xFFFFFFFF)
    {
        Sampler2D texture = textures[material.diffuseTextureIndex];
        float4 texel = texture.SampleLevel(uv, 0.0);
        baseColor *= texel.rgb;
        
        payload.style = 1;
    }
    
    if (material.normalTextureIndex != 0xFFFFFFFF)
    {
        Sampler2D texture = textures[material.normalTextureIndex];
        float3 texel = texture.SampleLevel(uv, 0.0).rgb;
        texel = normalize(texel * 2.0 - 1.0); // [0, 1] -> [-1, 1]
        
        float4 tangent = v0.tangent * b0 + v1.tangent * b1 + v2.tangent * b2;
        float handedness = tangent.w;
        
        float3x3 M = (float3x3)transform;
        float3 T = normalize(mul(M, tangent.xyz));
        T = normalize(T - dot(T, N) * N); // re-orthogonalize T with respect to N
        
        if (length(T) < 0.0001)
        {
            T = computeTangent(v0, v1, v2, (float3x3)transform, N);
        }
        
        float3 B = normalize(cross(N, T)) * handedness;

        N = normalize(T * texel.r + B * texel.g + N * texel.b);
    }
    
    float metallic = 0.0;
    float roughness = 1.0;
    if (material.metallicTextureIndex != 0xFFFFFFFF)
    {
        Sampler2D texture = textures[material.metallicTextureIndex];
        float3 texel = texture.SampleLevel(uv, 0.0).rgb;
        
        metallic = texel.b;
        roughness = texel.g;
    }
    
    payload.hit = true;
    payload.albedo = baseColor;
    payload.emission = material.emissive;
    if (length(payload.emission) > 0.001)
    {
        payload.terminate = false; //true; keep this false for now but know that this is tunable
    }
    
    payload.normal = N;
    payload.hitPos = worldPos;
    payload.mr = float2(metallic, roughness);
    payload.specular = material.specular.w;

}

[shader("closesthit")]
void shadowhitMain(inout ShadowPayload payload)
{
    payload.hit = true;
}