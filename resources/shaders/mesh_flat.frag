#version 450

#extension GL_KHR_vulkan_glsl:enable

layout (location = 0) in PerVertexData{
	vec2 uv;
	vec3 color;
	vec3 pos; // worldPos
	vec3 normal;
} fragIn;

layout (location = 0) out vec4 fs_Color;

layout(set = 1, binding = 7) uniform sampler2DArray compressedSampler;

void CoordinateSystem(in vec3 normal, out vec3 tangent, out vec3 bitangent)
{
	vec3 up = abs(normal.z) < 0.999f ? vec3(0.f, 0.f, 1.f) : vec3(1.f, 0.f, 0.f);
	tangent = normalize(cross(up, normal));
	bitangent = cross(normal, tangent);
}

vec3 GetNormal()
{	
	vec3 normal = normalize(fragIn.normal);
	vec3 nor = texture(compressedSampler, vec3(fragIn.uv, 1.0)).rgb;
	nor = 2 * nor - vec3(1.0f);
	
	if(dot(nor, nor) > 0.f)
	{
		vec3 tangent;
		vec3 bitangent;
		CoordinateSystem(normal, tangent, bitangent);
	
		normal = normalize(nor.x * tangent + nor.y * bitangent + nor.z * normal);
	}

	return normal;
	
}

void main()
{
	vec3 nor = GetNormal();
	//fs_Color = vec3(nor * 0.5f + 0.5f);//texture(u_Texture, vec3(fragIn.uv, 0.f)).rgb;
	//fs_Color = fragIn.color;

	fs_Color = vec4(nor * 0.5f + 0.5f, 1.f);
}