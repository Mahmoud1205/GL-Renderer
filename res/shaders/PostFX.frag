#version 460 core

#include "Utils.glsl"

out vec4 FragColor;

in vec2 UV;

layout (binding = 0) uniform sampler2D	uScreenTexture;
layout (binding = 1) uniform sampler2D	uBloomTexture;
layout (binding = 2) uniform sampler2D	uLensDirtTexture;
layout (binding = 3) uniform sampler3D	uCLUT;

uniform bool uEnableCLUT;

layout (binding = 0, std430) buffer	Luma {
	uint	TotalLuma;			// not used by this shader
	float	DesiredExposure;	// not used by this shader
	float	Exposure;			// the current exposure
};

vec3 AcesTonemap(vec3 x);
vec3 Uncharted2Tonemap(vec3 x);
vec3 LoglTonemap(vec3 x);

void main()
{
	const float gamma = 2.2;
	vec3 color = texture(uScreenTexture, UV).rgb;

	vec3 mapped = vec3(0.0);
	// mapped = AcesTonemap(color * Exposure);
	// mapped = Uncharted2Tonemap(color);
	mapped = LoglTonemap(color);

	mapped = pow(mapped, vec3(1.0 / gamma));

	// if (uEnableCLUT)
		// mapped = texture(uCLUT, mapped).rgb;

	vec3 bloom = texture(uBloomTexture, UV).rgb;

	vec3 dirt = texture(uLensDirtTexture, vec2(UV.x, 1.0 - UV.y)).rgb * 1.5;

	float blurFactor = 0.057;
	vec3 result = mix(mapped, bloom + bloom * dirt, blurFactor);

	if (uEnableCLUT)
		result = texture(uCLUT, result).rgb;

	FragColor = vec4(result, 1.0);
}

vec3 AcesTonemap(vec3 x)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 Uncharted2Tonemap(vec3 x)
{
	const float a = 0.15;
	const float b = 0.50;
	const float c = 0.10;
	const float d = 0.20;
	const float e = 0.02;
	const float f = 0.30;
	const float w = 11.2;

	return (((x * (a * x + c * b) + d * e) / (x * (a * x + b) + d * f)) - e / f) * Exposure;
}

vec3 LoglTonemap(vec3 x)
{
	return vec3(1.0) - exp(-x * Exposure);
}
