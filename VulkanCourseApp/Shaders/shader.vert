#version 450 		// Use GLSL 4.5

//layout(location = 0) out vec3 fragColor; // Output color for vertex (location is required)

//// Triangle vertex positions (will put in to vertex buffer later!)
//vec3 positions[3] = vec3[] (
//	vec3(0.0, -0.4, 0.0),
//	vec3(0.4, 0.4, 0.0),
//	vec3(-0.4, 0.4, 0.0)
//);
//
//// Triangle vertex colors
//vec3 colors[3] = vec3[](
//	vec3(1.0, 0.0, 0.0),
//	vec3(0.0, 1.0, 0.0),
//	vec3(0.0, 0.0, 1.0)
//);

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 col;

layout(location = 0) out vec3 fragCol;

void main()
{
	gl_Position = vec4(pos, 1.0);
	fragCol = col;
}