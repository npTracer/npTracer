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

float3 stylizeToon(float3 sample, float3 normal, float3 wo, float3 lightColor, float3 shadowColor, uint32_t numBands)
{
    float nDotV = dot(normal, wo);
    
    numBands = max(numBands, 1u);

    float currBand = floor(nDotV * numBands);
    currBand = min(currBand, numBands - 1);

    float t = currBand / max(float(numBands - 1), 1.f);

    return lerp(lightColor, shadowColor, t);
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

float planeDistance(float3 position, float3 n, float s)
{
    float u = dot(position, n);
    return abs(u - s * round(u / s));
}

float3 stylizeCrossHatch(float3 sample, float3 position)
{
    float lum = luminance(sample);

    const float spacing  = 0.02;   // distance between adjacent slice planes
    const float width    = 0.007;  // hatch line thickness
    const float3 lineCol = float3(0.04, 0.03, 0.02);
    const float3 bgCol   = float3(0.95, 0.93, 0.88); 

    const float t0 = 0.75;
    const float t1 = 0.45;
    const float t2 = 0.18;

    const float3 n1 = normalize(float3( 1.0,  1.0, 0.2));
    const float3 n2 = normalize(float3( 1.0, -1.0, 0.2));
    const float3 n3 = normalize(float3( 0.0,  1.0, 0.0));

    float halfW = width * 0.5;
    bool on1 = (lum < t0) && (planeDistance(position, n1, spacing) < halfW);
    bool on2 = (lum < t1) && (planeDistance(position, n2, spacing) < halfW);
    bool on3 = (lum < t2) && (planeDistance(position, n3, spacing * 0.6) < halfW);

    return (on1 || on2 || on3) ? lineCol : bgCol;
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
        return stylizeToon(L, N, wo, float3(1.f, 0.f, 0.f), float3(0.f, 1.f, 0.f), 3u);
    }
    else if (style == 3u)
    {
        return stylizeStripes(L, pos);
    }
    else if (style == 4u)
    {
        return stylizeCrossHatch(L, pos);
    }
    
    return L;
}