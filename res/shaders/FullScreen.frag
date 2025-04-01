#version 460 core

out vec4 FragColor;

in vec2 UV;

layout (binding = 0) uniform sampler2D uScreenTexture;

void main()
{
	FragColor = vec4(texture(uScreenTexture, UV).rgb, 1.0);
}
