struct VertexInput
{
    float2 pos;
    float3 color;
}

struct VertexOutput 
{
	float3 color;
	float4 sv_position : SV_Position;
}

[shader("vertex")]
VertexOutput vertMain(VertexInput vertex)
{
	VertexOutput output;
	output.sv_position = float4(vertex.pos, 0.0, 1.0);
	output.color = vertex.color;
	return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput vertex) : SV_Target
{
	return float4(vertex.color, 1.0);
}