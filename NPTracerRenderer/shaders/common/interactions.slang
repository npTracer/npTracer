import structs;
import utils;

// returns f_r(x, wo, wi) and 1 / pdf in w component
float4 lambertianBRDF(float3 N, float3 wi, float3 albedo)
{
    float Ndot = max(dot(N, wi), 0.0);
    float pdf = Ndot / 3.14159265;
    float3 f = albedo / 3.14159265; // lambertian
    return float4(f, 1 / pdf);
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float DGGX(float NoH, float roughness)
{
    roughness = max(roughness, 0.04);
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float NoH2 = NoH * NoH;
    float b = NoH2 * (alpha2 - 1.0) + 1.0;
    return alpha2 / (3.14159265 * b * b);
}

float G1GGXSchlick(float NoV, float roughness)
{
    float alpha = roughness * roughness;
    float k = alpha / 2.0;
    return max(NoV, 0.001) / (NoV * (1.0 - k) + k);
}

float GSmith(float NoV, float NoL, float roughness)
{
    return G1GGXSchlick(NoL, roughness) * G1GGXSchlick(NoV, roughness);
}

float3 sampleGGXHalfVector(float2 u, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;

    float phi = 2.0 * 3.14159265 * u.x;

    float cosTheta = sqrt((1.0 - u.y) / (1.0 + (alpha2 - 1.0) * u.y));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));

    float3 Ht;
    Ht.x = sinTheta * cos(phi);
    Ht.y = sinTheta * sin(phi);
    Ht.z = cosTheta;
    return normalize(Ht);    
}

float pdfCosineHemisphere(float NoL)
{
    return NoL / 3.14159265;
}

float pdfGGXReflection(float NoH, float VoH, float roughness)
{
    float D = DGGX(NoH, roughness);
    return max(D * NoH / max(4.0 * VoH, 1e-6), 1e-6);
}

BRDFSample sampleMicrofacetBRDF(float3 N, float3 wo, float3 albedo, float2 mr, float specular, inout uint32_t seed)
{
    BRDFSample s;
    s.wi = float3(0.0, 0.0, 0.0);
    s.weight = float3(0.0, 0.0, 0.0);
    
    float metallic = saturate(mr.x);
    float roughness = max(mr.y, 0.04);   
    
    float NoV = saturate(dot(N, wo));
    if (NoV <= 0.0) return s;
    
    float3 f0 = float3(0.16 * (specular * specular)); 
    f0 = lerp(f0, albedo, metallic);
    
    float diffuseLobe = (1.0 - metallic);
    float specularLobe = 1.0;
    
    float sum = diffuseLobe + specularLobe;
    float pDiffuse = diffuseLobe / sum;
    float pSpecular = specularLobe / sum;
    
    float3 wi = float3(0.0);
    float choose = rand(seed);
    
    if (choose < pDiffuse)
    {
        float3 wiLocal = cosineSampleHemisphere(seed);
        wi = transformWorld(wiLocal, N);
    }
    else
    {
        float2 u = float2(rand(seed), rand(seed));

        float3 Hlocal = sampleGGXHalfVector(u, roughness);
        float3 H = transformWorld(Hlocal, N);

        wi = reflect(-wo, H);

        if (dot(N, wi) <= 0.0)
            return s;
    }
    
    float NoL = saturate(dot(N, wi));
    if (NoL <= 0.0) return s;
    
    float3 Hsum = wo + wi;
    float len2 = dot(Hsum, Hsum);
    if (len2 < 1e-8) return s;
    float3 H = Hsum * rsqrt(len2);
    
    float NoH = saturate(dot(N, H));
    float VoH = saturate(dot(wo, H));
    
    float pdfDiffuse = pdfCosineHemisphere(NoL);
    float pdfSpecular = pdfGGXReflection(NoH, VoH, roughness);
    
    float pdf = pDiffuse * pdfDiffuse + pSpecular * pdfSpecular;
    if (pdf <= 1e-6) return s;
    
    float3 f = evalMicrofacetBRDF(N, wo, wi, albedo, mr, specular);
    
    s.wi = wi;
    s.weight = f * NoL / pdf;
    return s;
}

// returns f_r(x, wo, wi) and 1 / pdf in w component
float3 evalMicrofacetBRDF(float3 N, float3 wo, float3 wi, float3 albedo, float2 mr, float specular)
{
    float metallic = saturate(mr.x);
    float roughness = max(mr.y, 0.04);

    float3 H = normalize(wo + wi); 
    
    float NoV = clamp(dot(N, wo), 0.0, 1.0); 
    float NoL = clamp(dot(N, wi), 0.0, 1.0); 
    float NoH = clamp(dot(N, H), 0.0, 1.0); 
    float VoH = clamp(dot(wo, H), 0.0, 1.0); 
    
    float3 f0 = float3(0.16 * (specular * specular)); 
    f0 = lerp(f0, albedo, metallic); 
    
    float3 F = fresnelSchlick(VoH, f0); 
    float D = DGGX(NoH, roughness); 
    float G = GSmith(NoV, NoL, roughness); 
    
    float3 spec = (F * D * G) / (4.0 * max(NoV, 0.001) * max(NoL, 0.001)); 
    
    float3 kD = (1.0 - F) * (1.0 - metallic); 
    float3 diffuse = kD * albedo / 3.14159265; 
    
    return diffuse + spec;
}