// returns f_r(x, wo, wi) and 1 / pdf in w component
float4 lambertianBRDF(float3 N, float3 wi, float3 albedo)
{
    float Ndot = max(dot(N, wi), 0.0);
    float pdf = Ndot / 3.14159265;
    float3 f = albedo / 3.14159265; // lambertian
    return float4(f, 1 / pdf);
}