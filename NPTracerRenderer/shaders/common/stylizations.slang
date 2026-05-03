float luminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float3 identity(float3 color)
{
    return float3(1.0, 1.0, 1.0);
}

float3 stylizeGreyScale(float3 sample)
{
    float lum = luminance(sample);
    return float3(lum, lum, lum);
}

float3 stylizeContour(float3 sample, float3 normal, float3 wo)
{
    float NoV = dot(normal, wo);
    
    if (abs(NoV) < 0.33)
    {
        return float3(0.01, 0.05, 0.2);
    }
    else if (abs(NoV) < 0.66)
    {
        return float3(0.05, 0.2, 0.6);
    }
    
    return float3(0.15, 0.5, 1.0);
}

float3 stylizeStripes(float3 sample, float3 position)
{
    float3 purpleShade = float3(0.81, 0.33, 0.88);
    float3 yellowShade = float3(0.96, 0.945, 0.07);
    
    float stripePat = 0.0;
    
    if ((position.y > 0 && position.y - (int)position.y < 0.5) ||
        (position.y < 0 && (int)position.y - position.y > 0.5))
    {
        stripePat = 1.0;
    }
    
    float3 patternShade = purpleShade * stripePat + yellowShade * (1 - stripePat);
    return patternShade;
}

float3 stylizeToon(float3 sample)
{
    float luminance = luminance(sample);
    
    float3 toonRamp = float3(0.55, 0.48, 0.67);
    
    float intensity = smoothstep(0.05, 0.10, luminance);
    return toonRamp * intensity;
}

// TODO: make a JSON style map data file and corresponding Python automation script
float3 stylize(float style, float3 L, float3 N, float3 wo, float3 pos)
{
    if (style == 1u)
    {
        return stylizeGreyScale(L);
    }
    else if (style == 2u)
    {
        return stylizeContour(L, N, wo);
    }
    else if (style == 3u)
    {
        return stylizeStripes(L, pos);
    }
    else if (style == 4u)
    {
        return stylizeToon(L);
    }
    
    return L;
}