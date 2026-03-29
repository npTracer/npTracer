struct VertexOutput
{
    float4 pos : SV_Position;
};

[shader("vertex")]
VertexOutput vertMain(uint vertexID : SV_VertexID)
{
    VertexOutput output;

    // Fullscreen triangle
    float2 positions[3] =
    {
        float2(-1.0, -1.0),
        float2( 0.0, -1.0),
        float2(-1.0,  0.0)
    };

    output.pos = float4(positions[vertexID], 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput input) : SV_Target
{
    return float4(1.0, 0.0, 0.0, 1.0);
}