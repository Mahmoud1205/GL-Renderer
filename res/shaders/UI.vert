#version 460 core

layout (location = 0) in vec4 aVertex; // vec2 pos and vec2 UV

out vec2 UV;

uniform mat4 uProjection;

void main()
{
	gl_Position = uProjection * vec4(aVertex.xy, 0.0, 1.0);
	UV = aVertex.zw;
}
