#version 450

//shader input
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec3 pos;
layout (location = 2) in vec3 localPos;
//output write
layout (location = 0) out vec4 outFragColor;




void main() 
{
	float dist = 1.0 - distance(pos, localPos);
	dist = step(0.9, dist);
	if(dist < 0.9) {
		discard;
	}
	vec3 distColor = vec3(dist) * inColor;
	outFragColor = vec4(distColor,1.0f);
//	outFragColor = vec4(inColor, 1.0);
}
