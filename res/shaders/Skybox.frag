#version 460 core

out vec4 FragColor;

in vec3 UVW;

layout (binding = 0, std430) buffer	Luma {
	uint	TotalLuma;			// not used by this shader
	float	DesiredExposure;	// not used by this shader
	float	Exposure;			// the current exposure
};

layout (binding = 1) uniform samplerCube uSkybox;

// a good tonemap function for many skyboxs
vec3 SkyboxTonemap(vec3 x)
{
	float exposure = Exposure + 1.46;
	vec3 result = vec3(1.0) - exp(-x * exposure);
	result = pow(result, vec3(1.39));
	return result;
}

void main()
{
	vec3 result = texture(uSkybox, UVW).rgb;
	result = SkyboxTonemap(result);
	result = pow(result, vec3(2.2));

	FragColor = vec4(result, 1.0);
}
