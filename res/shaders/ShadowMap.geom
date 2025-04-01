#version 460 core

// invocations = cascade count
layout (triangles, invocations = 4) in;
layout (triangle_strip, max_vertices = 3) out;

uniform mat4 uCascadeMatrices[4];

void main()
{
	for (uint i = 0; i < 3; i++)
	{
		gl_Position = uCascadeMatrices[gl_InvocationID] * gl_in[i].gl_Position;
		gl_Layer = gl_InvocationID;
		EmitVertex();
	}
	EndPrimitive();
}
