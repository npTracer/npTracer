float luminance(float3 samples)
{
    return dot(samples, float3(0.2126, 0.7152, 0.0722));
}

float3 stylizeGreyScale(float3 samples)
{
    let lum = luminance(samples);
    return float3(lum, lum, lum);
}