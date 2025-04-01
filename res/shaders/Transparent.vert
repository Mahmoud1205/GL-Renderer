#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

out vec3 Normal;
out vec2 UV;
out vec3 FragPos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main()
{
	Normal = aNormal;
	UV = aUV;

	vec4 worldPos = uModel * vec4(aPos, 1.0);
	FragPos = worldPos.xyz;

	gl_Position = uProjection * uView * worldPos;
}
