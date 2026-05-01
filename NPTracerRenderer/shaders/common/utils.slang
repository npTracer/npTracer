import structs;

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

float3 cosineSampleHemisphere(inout uint32_t seed)
{
    float2 xi = float2(rand(seed), rand(seed));
    
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

float3 computeTangent(Vertex v0, Vertex v1, Vertex v2, float3x3 transform, float3 N)
{
    float3 T;

    float3 e1 = v1.pos.xyz - v0.pos.xyz;
    float3 e2 = v2.pos.xyz - v0.pos.xyz;
    float2 dUV1 = v1.uv - v0.uv;
    float2 dUV2 = v2.uv - v0.uv;
    
    float det = dUV1.x * dUV2.y - dUV2.x * dUV1.y; // determinant
    if (abs(det) > 0.0001)
    {
        T = normalize(mul((float3x3)transform, (e1 * dUV2.y - e2 * dUV1.y) * (1.0 / det)));
    }
    else // default to arbitrary basis
    {
        float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
        T = normalize(cross(up, N));
    }
    
    return T;
}