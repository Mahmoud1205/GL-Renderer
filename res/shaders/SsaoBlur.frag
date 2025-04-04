#version 460 core

out float FragColor;

in vec2 UV;

layout (binding = 0) uniform sampler2D uOcclusionTexture;

void main()
{
	vec2 texelSize = 1.0 / vec2(textureSize(uOcclusionTexture, 0));
	float result = 0.0;
	for (int x = -2; x < 2; ++x) 
	{
		for (int y = -2; y < 2; ++y) 
		{
			vec2 offset = vec2(float(x), float(y)) * texelSize;
			result += texture(uOcclusionTexture, UV + offset).r;
		}
	}
	FragColor = result / (4.0 * 4.0);
}
