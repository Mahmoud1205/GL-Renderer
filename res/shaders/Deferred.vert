#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out vec3 Normal;
out vec2 UV;
out vec3 FragPos;
out mat3 TBN;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main()
{
	// Note: when you multiply normals by a matrix, the normal.w mustnt
	//		 be 1 like a position vector, cuz normals are direction vectors
	//		 the normal.w must be 0
	Normal = (uView * vec4(aNormal, 0.0)).xyz;
	UV = aUV;

	vec3 T = normalize(vec3(uModel * vec4(aTangent, 0.0)));
	vec3 B = normalize(vec3(uModel * vec4(aBitangent, 0.0)));
	vec3 N = normalize(vec3(uModel * vec4(Normal, 0.0)));
	TBN = mat3(T, B, N);

	vec4 viewPos = uView * uModel * vec4(aPos, 1.0);
	FragPos = viewPos.xyz;

	gl_Position = uProjection * viewPos;
}
