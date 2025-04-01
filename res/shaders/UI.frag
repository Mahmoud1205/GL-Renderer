#version 460 core

in vec2 UV;

out vec4 FragColor;

layout (binding = 0) uniform sampler2D uComponent;

uniform vec3 uTint;

void main()
{
	vec4 color = vec4(1.0, 1.0, 1.0, texture(uComponent, UV).r);
	FragColor = vec4(uTint, 1.0) * color;
}
