#version 460 core

out float FragColor;

in vec2 UV;

#define SAMPLE_COUNT (32)

layout (binding = 0) uniform sampler2D uPositionTexture;
layout (binding = 1) uniform sampler2D uNormalTexture;
layout (binding = 2) uniform sampler2D uNoiseTexture;

uniform vec3 uSamples[SAMPLE_COUNT];
uniform mat4 uProjection;

vec2 gNoiseScale;

const float kRadius = 3.1;
const float kBias = 0.025;

void main()
{
	float noiseSize = textureSize(uNoiseTexture, 0).x;
	vec2 screenSize = textureSize(uPositionTexture, 0).xy;

	gNoiseScale = vec2(screenSize.x / noiseSize, screenSize.y / noiseSize);

	vec3 pos		= texture(uPositionTexture, UV).rgb;
	vec3 normal		= texture(uNormalTexture, UV).rgb;
	vec3 randomVec	= texture(uNoiseTexture, UV * gNoiseScale).rgb;

	vec3 tangent	= normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent	= cross(normal, tangent);
	mat3 TBN		= mat3(tangent, bitangent, normal);

	float occlusion = 0.0;
	for (uint i = 0; i < SAMPLE_COUNT; i++)
	{
		vec3 samplePos = TBN * uSamples[i];
		samplePos = pos + samplePos * kRadius;

		vec4 offset = vec4(samplePos, 1.0);
		offset = uProjection * offset;
		offset.xyz /= offset.w;
		offset.xyz = offset.xyz * 0.5 + 0.5;

		if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0)
			continue;

		float sampleDepth = texture(uPositionTexture, offset.xy).z;

		float rangeCheck = smoothstep(0.0, 1.0, kRadius / abs(pos.z - sampleDepth));
		occlusion += (sampleDepth >= samplePos.z + kBias ? 1.0 : 0.0) * rangeCheck;
	}

	occlusion = 1.0 - (occlusion / float(SAMPLE_COUNT));
	FragColor = occlusion;
}
