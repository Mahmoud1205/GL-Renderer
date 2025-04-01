#version 460 core

#include "Utils.glsl"

/**
 * reference paper. read for the `#define`s:
 * https://developer.download.nvidia.com/assets/gamedev/files/sdk/11/FXAA_WhitePaper.pdf
 */

out vec4 FragColor;

in vec2 UV;

layout (binding = 0) uniform sampler2D	uScreenTexture;
uniform bool							uEnableFxaa;

vec3 FxaaTextureOffset(vec2 pos, ivec2 offset);

#define FXAA_EDGE_THRESHOLD (1.0 / 8.0)
#define FXAA_EDGE_THRESHOLD_MIN (1.0 / 16.0)
#define FXAA_SUBPIX_TRIM (1.0 / 4.0)
#define FXAA_SUBPIX_TRIM_SCALE (1)
#define FXAA_SUBPIX_CAP (3.0 / 4.0)

// the paper says, those defines here are the most impactful to performance,
// tune them.plz
#define FXAA_SEARCH_STEPS (8)
#define FXAA_SEARCH_THRESHOLD (1.0 / 4.0)

vec2 gTexelSize;

void main()
{
	if (!uEnableFxaa)
	{
		FragColor = vec4(texture(uScreenTexture, UV).rgb, 1.0);
		return;
	}

	gTexelSize = 1.0 / textureSize(uScreenTexture, 0);

	vec3 rgbM = texture(uScreenTexture, UV).rgb;
	vec3 rgbN = FxaaTextureOffset(UV, ivec2( 0, -1));
	vec3 rgbW = FxaaTextureOffset(UV, ivec2(-1,  0));
	vec3 rgbS = FxaaTextureOffset(UV, ivec2( 0,  1));
	vec3 rgbE = FxaaTextureOffset(UV, ivec2( 1,  0));

	float lumM = Luminance(rgbM);
	float lumN = Luminance(rgbN);
	float lumW = Luminance(rgbW);
	float lumS = Luminance(rgbS);
	float lumE = Luminance(rgbE);

	float rangeMin = min(lumM, min(min(lumN, lumW), min(lumS, lumE)));
	float rangeMax = max(lumM, max(max(lumN, lumW), max(lumS, lumE)));
	float range = rangeMax - rangeMin;
	if (range < max(FXAA_EDGE_THRESHOLD_MIN, rangeMax * FXAA_EDGE_THRESHOLD))
	{
		FragColor = vec4(rgbM, 1.0);
		return;
	}

	float lumL = (lumN + lumW + lumS + lumE) * 0.25;
	float rangeL = abs(lumL - lumM);
	float blendL = max(0.0, (rangeL / range) - FXAA_SUBPIX_TRIM) * FXAA_SUBPIX_TRIM_SCALE;
	blendL = min(FXAA_SUBPIX_CAP, blendL);

	vec3 rgbL = rgbN + rgbW + rgbM + rgbE + rgbS;
	vec3 rgbNW = FxaaTextureOffset(UV, ivec2(-1, -1));
	vec3 rgbNE = FxaaTextureOffset(UV, ivec2( 1, -1));
	vec3 rgbSW = FxaaTextureOffset(UV, ivec2(-1,  1));
	vec3 rgbSE = FxaaTextureOffset(UV, ivec2( 1,  1));
	float lumNW = Luminance(rgbNW);
	float lumNE = Luminance(rgbNE);
	float lumSW = Luminance(rgbSW);
	float lumSE = Luminance(rgbSE);

	rgbL += (rgbNW + rgbNE + rgbSW + rgbSE);
	rgbL *= vec3(1.0 / 9.0);

	/**
	 * imagine those below as
	 * NW N  NE
	 *  \ | /
	 *   \|/
	 *    .
	 *
	 *    NW
	 *      \
	 *  W____\
	 *       /
	 *      /
	 *     SW
	 *
	 * etc
	 */
	float edgeVert =
		abs((0.25 * lumNW) + (-0.5 * lumN) + (0.25 * lumNE)) +
		abs((0.50 * lumW ) + (-1.0 * lumM) + (0.50 * lumE )) +
		abs((0.25 * lumSW) + (-0.5 * lumS) + (0.25 * lumSE));

	float edgeHorz =
		abs((0.25 * lumNW) + (-0.5 * lumW) + (0.25 * lumSW)) +
		abs((0.50 * lumN ) + (-1.0 * lumM) + (0.50 * lumS )) +
		abs((0.25 * lumNE) + (-0.5 * lumE) + (0.25 * lumSE));

	bool horzSpan = edgeHorz >= edgeVert;

	float lengthSign = horzSpan ? -gTexelSize.y : -gTexelSize.x;
	if (!horzSpan)
	{
		lumN = lumW;
		lumS = lumE;
	}

	float gradN = abs(lumN - lumM);
	float gradS = abs(lumS - lumM);
	lumN = (lumN + lumM) / 2.0;
	lumS = (lumS + lumM) / 2.0;

	bool pairN = gradN >= gradS;

	if (!pairN)
	{
		lumN = lumS;
		gradN = gradS;
		lengthSign *= -1.0;
	}

	vec2 posN;
	posN.x = UV.x + (horzSpan ? 0.0 : lengthSign / 2.0);
	posN.y = UV.y + (horzSpan ? lengthSign / 2.0 : 0.0);

	gradN *= FXAA_SEARCH_THRESHOLD;

	vec2 posP = posN;
	vec2 offsetNP = horzSpan ? vec2(gTexelSize.x, 0.0) : vec2(0.0, gTexelSize.y);
	float lumEndN = lumN;
	float lumEndP = lumN;

	bool doneN = false;
	bool doneP = false;

	posN += offsetNP * vec2(-2.5, -2.5);
	posP += offsetNP * vec2( 2.5,  2.5);
	offsetNP *= vec2(4.0, 4.0);

	for (uint i = 0; i < FXAA_SEARCH_STEPS; i++)
	{
		if (!doneN) lumEndN = Luminance(textureGrad(uScreenTexture, posN, offsetNP, offsetNP).rgb);
		if (!doneP) lumEndP = Luminance(textureGrad(uScreenTexture, posP, offsetNP, offsetNP).rgb);

		doneN = doneN || (abs(lumEndN - lumN) >= gradN);
		doneP = doneP || (abs(lumEndP - lumN) >= gradN);

		if (doneN && doneP) break;

		if (!doneN) posN -= offsetNP;
		if (!doneP) posP += offsetNP;
	}

	float distN = horzSpan ? (UV.x - posN.x) : (UV.y - posN.y);
	float distP = horzSpan ? (posP.x - UV.x) : (posP.y - UV.y);

	bool directionN = distN < distP;

	lumEndN = directionN ? lumEndN : lumEndP;

	if (((lumM - lumN) < 0.0) == ((lumEndN - lumN) < 0.0))
		lengthSign = 0.0;

	float spanLength = (distP + distN);
	distN = directionN ? distN : distP;
	float subPixelOffset = (0.5 + (distN * (-1.0 / spanLength))) * lengthSign;

	vec2 sampleFrom;
	sampleFrom.x = UV.x + (horzSpan ? 0.0 : subPixelOffset);
	sampleFrom.y = UV.y + (horzSpan ? subPixelOffset : 0.0);

	vec3 rgbF = texture(uScreenTexture, sampleFrom).xyz;

	FragColor = vec4(rgbF, 1.0);
}

vec3 FxaaTextureOffset(vec2 pos, ivec2 offset)
{
	return texture(uScreenTexture, clamp(pos + offset * gTexelSize, 0.0, 1.0)).rgb;
}
