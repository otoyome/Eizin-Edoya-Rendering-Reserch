#include "particle.hlsli"

StructuredBuffer<particle> particle_buffer : register(t9);

[maxvertexcount(4)]
void main(point VS_OUT input[1], inout TriangleStream<GS_OUT> output)
{
	const float2 corners[] =
	{
		float2(-1.0, -1.0),
		float2(-1.0, +1.0),
		float2(+1.0, -1.0),
		float2(+1.0, +1.0),
	};
	const float2 texcoords[] =
	{
		float2(0.0, 1.0),
		float2(0.0, 0.0),
		float2(1.0, 1.0),
		float2(1.0, 0.0),
	};
	
	particle p = particle_buffer[input[0].vertex_id];

	const float aspect_ratio = 1280.0 / 720.0;

	float2 size = p.size;
	
	
	float3 view_space_velocity = mul(float4(p.velocity, 0.0f), view).xyz;
	float4 view_space_pos = mul(float4(p.position, 1.0), view);

	[unroll]
	for (uint vertex_index = 0; vertex_index < 4; ++vertex_index)
	{
		GS_OUT element;

		// Transform to clip space
		element.position = mul(float4(p.position, 1), view_projection);
		// Make corner position as billboard
		float2 corner;
		corner = corners[vertex_index];
		
		float2 streak = max(0, dot(normalize(-view_space_velocity), normalize(float3(corner, 0.0)))) * -view_space_velocity;
		float streak_factor = 30.0;
		corner = corner + streak_factor * streak;

		element.position.xy += corner * float2(size.x, size.y * aspect_ratio);
		element.color = p.color;
		element.texcoord = texcoords[vertex_index];
		output.Append(element);
	}
	output.RestartStrip();
}
