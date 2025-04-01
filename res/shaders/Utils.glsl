float Luminance(vec3 color)
{
	return color.g * (0.587 / 0.299) + color.r;
}

vec2 Saturate(vec2 col)
{
	return clamp(col, 0.0, 1.0);
}

vec3 Saturate(vec3 col)
{
	return clamp(col, 0.0, 1.0);
}

bool IsUnsaturated(vec2 a)
{
	if (a.x < 0.0 || a.x > 1.0 || a.y < 0.0 || a.y > 1.0)
		return true;
	return false;
}

float InvertRange(float val, float rangeStart, float rangeEnd)
{
	if (rangeStart > rangeEnd)
	{
		float tmp = rangeStart;
		rangeStart = rangeEnd;
		rangeEnd = tmp;
	}
	return rangeStart + rangeEnd - val;
}

float LinearizeDepth(float depth, float near, float far) 
{
	float z = depth * 2.0 - 1.0;
	return (2.0 * near * far) / (far + near - z * (far - near)) / far;
}
