
cbuffer PARTICLE_SYSTEM_CONSTANTS : register(b9)
{
	float4 emission_position;
	float time;
	float delta_time;
	uint max_particle_count;
}

cbuffer SCENE_CONSTANT_BUFFER : register(b1)
{
	row_major float4x4 view_projection;
	float4 light_direction;
	float4 camera_position;
	// PARTICLES
	row_major float4x4 view;
	row_major float4x4 projection;
}

struct VS_OUT
{
	uint vertex_id : VERTEXID;
};
struct GS_OUT
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
	float2 texcoord : TEXCOORD;
};

struct particle
{
	float4 color;
	float3 position; // World space position
	float3 velocity; // World space velocity
	float size;
};

#define PI 3.14159265358979
float rand(float n)
{
	// Return value is greater than or equal to 0 and less than 1.
	return frac(sin(n) * 43758.5453123);
}


#define NUMTHREADS_X 16

void spawn(in uint id, inout particle p)
{
	float f0 = rand((float) id / max_particle_count * 2.0 * PI);
	float f1 = rand(f0 * 2.0 * PI);

	// Generate latitude theta in the range [0, ƒÎ]
	float theta = acos(2.0 * f0 - 1.0);
    // Generate longitude phi in the range [0, 2ƒÎ]
	float phi = 2.0 * PI * f1;
	
	float sin_theta = sin(theta);
	float cos_theta = cos(theta);
	float sin_phi = sin(phi);
	float cos_phi = cos(phi);
	
	float radius = 5.0;
	p.position.x = emission_position.x + radius * sin_theta * cos_phi;
	p.position.y = emission_position.y + radius * sin_theta * sin_phi;
	p.position.z = emission_position.z + radius * cos_theta;
	
	p.velocity.x = 0.0;
	p.velocity.y = 0.0;
	p.velocity.z = 0.0;
	
	p.size = 0.005;
	p.color = float4(1.0, 1.0, 1.0, 1.0);
}
void respawn(in uint id, inout particle p)
{
	spawn(id, p);
}

