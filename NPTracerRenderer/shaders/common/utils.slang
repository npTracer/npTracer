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