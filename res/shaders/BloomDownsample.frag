#version 460 core

// https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom

#include "Utils.glsl"

layout (binding = 0) uniform sampler2D uScreenTexture;

in vec2 UV;

out vec3 FragColor;

vec2 gTexelSize;

void main()
{
	gTexelSize = 1.0 / textureSize(uScreenTexture, 0);
	float x = gTexelSize.x;
	float y = gTexelSize.y;

	/**
	 * Take 13 samples around current texel:
	 * a - b - c
	 * - j - k -
	 * d - e - f
	 * - l - m -
	 * g - h - i
	 * === ('e' is the current texel) ===
	 */

	// TODO: IMPORTANT: dont sample pixel if UV out of the screen. SSAO.frag has a example

	vec3 a = texture(uScreenTexture, Saturate(vec2(UV.x - 2 * x,	UV.y + 2 * y))).rgb;
	vec3 b = texture(uScreenTexture, Saturate(vec2(UV.x,			UV.y + 2 * y))).rgb;
	vec3 c = texture(uScreenTexture, Saturate(vec2(UV.x + 2 * x,	UV.y + 2 * y))).rgb;

	vec3 d = texture(uScreenTexture, Saturate(vec2(UV.x - 2 * x,	UV.y))).rgb;
	vec3 e = texture(uScreenTexture, Saturate(vec2(UV.x,			UV.y))).rgb;
	vec3 f = texture(uScreenTexture, Saturate(vec2(UV.x + 2 * x,	UV.y))).rgb;

	vec3 g = texture(uScreenTexture, Saturate(vec2(UV.x - 2 * x,	UV.y - 2 * y))).rgb;
	vec3 h = texture(uScreenTexture, Saturate(vec2(UV.x,			UV.y - 2 * y))).rgb;
	vec3 i = texture(uScreenTexture, Saturate(vec2(UV.x + 2 * x,	UV.y - 2 * y))).rgb;

	vec3 j = texture(uScreenTexture, Saturate(vec2(UV.x - x,		UV.y + y))).rgb;
	vec3 k = texture(uScreenTexture, Saturate(vec2(UV.x + x,		UV.y + y))).rgb;
	vec3 l = texture(uScreenTexture, Saturate(vec2(UV.x - x,		UV.y - y))).rgb;
	vec3 m = texture(uScreenTexture, Saturate(vec2(UV.x + x,		UV.y - y))).rgb;

	/**
	 * Apply weighted distribution:
	 * 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
	 * a,b,d,e * 0.125
	 * b,c,e,f * 0.125
	 * d,e,g,h * 0.125
	 * e,f,h,i * 0.125
	 * j,k,l,m * 0.5
	 * This shows 5 square areas that are being sampled. But some of them overlap,
	 * so to have an energy preserving downsample we need to make some adjustments.
	 * The weights are the distributed, so that the sum of j,k,l,m (e.g.)
	 * contribute 0.5 to the final color output. The code below is written
	 * to effectively yield this sum. We get:
	 * 0.125*5 + 0.03125*4 + 0.0625*4 = 1
	 */
	FragColor = e * 0.125;
	FragColor += (a + c + g + i) * 0.03125;
	FragColor += (b + d + f + h) * 0.0625;
	FragColor += (j + k + l + m) * 0.125;
	FragColor = max(FragColor, 0.0001);
}
