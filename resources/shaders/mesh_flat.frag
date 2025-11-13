#version 450

#extension GL_KHR_vulkan_glsl:enable

layout (location = 0) in PerVertexData
{
	vec2 uv;
	vec3 color;
} fragIn;

layout (location = 0) out vec4 fs_Color;

layout(set = 1, binding = 5) uniform sampler2DArray u_Texture;

void main()
{
	//fs_Color = texture(u_Texture, vec3(fragIn.uv, 0.f)).rgb;
	fs_Color =  vec4(fragIn.color, 1.0);

	//fs_Color = vec4(1.0, 1.0, 1.0, 1.0);
}