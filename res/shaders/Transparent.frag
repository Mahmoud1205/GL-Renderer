#version 460 core

out vec4 FragColor;

in vec2 UV;
in vec3 Normal;
in vec3 FragPos;

struct PointLight
{
	vec3	mPosition;
	vec3	mColor;
	float	mLinear;
	float	mQuadratic;
};

struct DirLight
{
	vec3 mDirection;
	vec3 mColor;
};

#define MAX_POINT_LIGHTS 42

layout (binding = 0) uniform sampler2D		uAlbedoTexture;
layout (binding = 1) uniform sampler2D		uSpecularTexture;
layout (binding = 2) uniform sampler2D		uTransparencyTexture;
layout (binding = 3) uniform sampler2DArray	uCascades;

uniform bool							uUseTransparencyTex;
uniform vec3							uViewPos;
uniform uint							uNumPointLights;
uniform PointLight[MAX_POINT_LIGHTS]	uPointLights;
uniform DirLight						uDirLight;
uniform mat4							uLightSpaceMatrix;
uniform mat4							uCascadeMatrices[4];
uniform float							uShadowCascadeLevels[3];

float CalculateShadow(vec4 pos, vec3 normal, vec3 lightDir, int cascadeIdx);

void main()
{
	vec4 color 		= texture(uAlbedoTexture, UV);
	vec3 albedo		= color.rgb;
	float specular	= texture(uSpecularTexture, UV).r;
	vec3 normal		= normalize(Normal);

	vec3 viewDir = normalize(uViewPos - FragPos);

	vec3 result = albedo * 0.006;

	{ // dir light
		const float kAmbientFactor = 0.05;
		vec3 ambient = uDirLight.mColor * kAmbientFactor * albedo;

		vec3 lightDir	= normalize(-uDirLight.mDirection);
		vec3 halfwayDir	= normalize(lightDir + viewDir);

		float depth = abs(gl_FragDepth);
		int cascadeIdx = -1;
		for (int i = 0; i < 3; i++)
		{
			if (depth < uShadowCascadeLevels[i])
			{
				cascadeIdx = i;
				break;
			}
		}
		if (cascadeIdx == -1)
		{
			cascadeIdx = 3;
		}

		vec4 posLightSpace = uCascadeMatrices[cascadeIdx] * vec4(FragPos, 1.0);
		float shadow = CalculateShadow(posLightSpace, normal, lightDir, cascadeIdx);

		float diff = max(dot(normal, lightDir), 0.0);
		// TODO: the 1.0 here should be shininess, how can i fix this?
		float spec = pow(max(dot(normal, halfwayDir), 0.0), 1.0);

		vec3 diffuse		= uDirLight.mColor * diff * albedo;
		vec3 lightSpecular	= spec * vec3(specular);

		diffuse *= 1.0 - shadow;
		lightSpecular *= 1.0 - shadow;

		vec3 lighting = (ambient + diffuse + lightSpecular);

		result += lighting;
	}

	for (uint i = 0; i < uNumPointLights; i++)
	{
		vec3 lightDir = normalize(uPointLights[i].mPosition - FragPos);
		float diff = max(dot(normal, lightDir), 0.0);

		vec3 halfwayDir = normalize(lightDir + viewDir);
		// TODO: the 1.0 here should be shininess, how can i fix this?
		float spec = pow(max(dot(normal, halfwayDir), 0.0), 1.0);

		float distance = length(uPointLights[i].mPosition - FragPos);
		float attenuation = 1.0 / (1.0 + uPointLights[i].mLinear * distance
								   + uPointLights[i].mQuadratic * (distance * distance));

		vec3 ambient = uPointLights[i].mColor * albedo;

		vec3 diffuse		= uPointLights[i].mColor * diff * albedo;
		vec3 lightSpecular	= spec * vec3(specular);

		ambient			*= attenuation;
		diffuse			*= attenuation;
		lightSpecular	*= attenuation;

		vec3 lighting = (ambient + diffuse + lightSpecular);

		result += lighting;
	}

	float opacity = 1.0;
	if (uUseTransparencyTex)
		opacity = texture(uTransparencyTexture, UV).r;
	else
		opacity = color.a;

	FragColor = vec4(result, opacity);
}

float CalculateShadow(vec4 pos, vec3 normal, vec3 lightDir, int cascadeIdx)
{
	// transform to ndc
	vec3 projCoords = pos.xyz / pos.w;

	// conv from ndc ([-1,1]) to [0,1] so it can
	// accurately sample from the depth buffer which is [0,1]
	projCoords = projCoords * 0.5 + 0.5;

	// dont shadow the fragments that arent covered by the shadow map
	if (projCoords.z > 1.0)
		return 0.0;

	float currentDepth = projCoords.z;

	float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
	if (cascadeIdx == 3)
		bias *= 1.0 / (1000.0 * 0.5);
	else
		bias *= 1.0 / (uShadowCascadeLevels[cascadeIdx] * 0.5);

	float shadow = 0.0;
	vec2 texelSize = 1.0 / vec2(textureSize(uCascades, 0));
	for (int x = -1; x <= 1; x++)
	{
		for (int y = -1; y <= 1; y++)
		{
			float pcfDepth = texture(uCascades, vec3(projCoords.xy + vec2(x, y) * texelSize, cascadeIdx)).r;
			shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
		}
	}
	shadow /= 9.0;

	return shadow;
}
