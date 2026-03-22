struct VertexInput
{
    float2 pos;
    float3 color;
}

struct VertexOutput 
{
	float3 color;
	float4 pos : SV_Position;
}

struct UniformBuffer
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;

[shader("vertex")]
VertexOutput vertMain(VertexInput vertex)
{
	VertexOutput output;
	output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(vertex.pos, 0.0, 1.0))));
	output.color = vertex.color;
	return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput vertex) : SV_Target
{
	return float4(vertex.color, 1.0);
}