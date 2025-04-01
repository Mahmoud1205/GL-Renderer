#version 460 core

#include "Utils.glsl"

layout (binding = 0) uniform sampler2D uScreenTexture;

uniform float uFilterRadius;

in vec2 UV;

out vec3 FragColor;

void main()
{
	float x = uFilterRadius;
	float y = uFilterRadius;

	/** Take 9 samples around current texel:
	 * a - b - c
	 * d - e - f
	 * g - h - i
	 * === ('e' is the current texel) ===
	 */
	 
	// TODO: IMPORTANT: dont sample pixel if UV out of the screen. SSAO.frag has a example

	vec3 a = texture(uScreenTexture, Saturate(vec2(UV.x - x,	UV.y + y))).rgb;
	vec3 b = texture(uScreenTexture, Saturate(vec2(UV.x,		UV.y + y))).rgb;
	vec3 c = texture(uScreenTexture, Saturate(vec2(UV.x + x,	UV.y + y))).rgb;

	vec3 d = texture(uScreenTexture, Saturate(vec2(UV.x - x,	UV.y))).rgb;
	vec3 e = texture(uScreenTexture, Saturate(vec2(UV.x,		UV.y))).rgb;
	vec3 f = texture(uScreenTexture, Saturate(vec2(UV.x + x,	UV.y))).rgb;

	vec3 g = texture(uScreenTexture, Saturate(vec2(UV.x - x,	UV.y - y))).rgb;
	vec3 h = texture(uScreenTexture, Saturate(vec2(UV.x,		UV.y - y))).rgb;
	vec3 i = texture(uScreenTexture, Saturate(vec2(UV.x + x,	UV.y - y))).rgb;

	/** Apply weighted distribution, by using a 3x3 tent filter:
	 *  1   | 1 2 1 |
	 * -- * | 2 4 2 |
	 * 16   | 1 2 1 |
	 */
	FragColor = e * 4.0;
	FragColor += (b + d + f + h) * 2.0;
	FragColor += (a + c + g + i);
	FragColor *= 1.0 / 16.0;
}
