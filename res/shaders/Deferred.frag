#version 460 core

layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec4 gSpecular;
layout (location = 2) out vec4 gNormal;
layout (location = 3) out vec4 gPosition;

in vec2 UV;
in vec3 FragPos;
in vec3 Normal;
in mat3 TBN;

layout (binding = 0) uniform sampler2D uTextureDiffuse;
layout (binding = 1) uniform sampler2D uTextureSpecular;
layout (binding = 2) uniform sampler2D uTextureTransparency;
layout (binding = 3) uniform sampler2D uTextureNormal;

void main()
{
	gAlbedo.rgb	= texture(uTextureDiffuse, UV).rgb;
	gSpecular	= vec4(texture(uTextureSpecular, UV).rgb, 1.0);
	// depth values are in gPosition.a
	gPosition	= vec4(FragPos, gl_FragCoord.z);

	vec3 texNormal	= texture(uTextureNormal, UV).xyz;
	texNormal		= texNormal * 2.0 - 1.0; // [0,1] to [-1,1]
	gNormal			= vec4(normalize(TBN * texNormal), 1.0);
}
