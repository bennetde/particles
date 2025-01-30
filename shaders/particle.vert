#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
layout (location = 0) out vec3 outColor;
layout (location = 1) out vec3 pos;
layout (location = 2) out vec3 localPos;

struct Particle {
	vec4 position;
	vec4 velocity;
	ivec4 color;
};

layout(buffer_reference, std430) readonly buffer ParticleBuffer {
	Particle particles[];
};

layout( push_constant ) uniform constants {
	mat4 viewProj;
	ParticleBuffer particleBuffer;
} ViewProjectionMatrix;


vec3 hsl2rgb( in vec3 c )
{
    vec3 rgb = clamp( abs(mod(c.x*6.0+vec3(0.0,4.0,2.0),6.0)-3.0)-1.0, 0.0, 1.0 );
    return c.z + c.y * (rgb-0.5)*(1.0-abs(2.0*c.z-1.0));
}

void main() 
{
	//const array of positions for the triangle
	const vec3 positions[3] = vec3[3](
		vec3(0.5f,0.5f, 0.0f),
		vec3(-0.5f,0.5f, 0.0f),
		vec3(0.f,-0.5f, 0.0f)
	);

	//const array of colors for the triangle
	const vec3 colors[3] = vec3[3](
		vec3(1.0f, 0.0f, 0.0f), //red
		vec3(0.0f, 1.0f, 0.0f), //green
		vec3(00.f, 0.0f, 1.0f)  //blue
	);

	Particle particle = ViewProjectionMatrix.particleBuffer.particles[gl_InstanceIndex];

	mat4 aMat4 = mat4(1.0, 0.0, 0.0, 0.0,  // 1. column
                  0.0, 1.0, 0.0, 0.0,  // 2. column
                  0.0, 0.0, 1.0, 0.0,  // 3. column
                  particle.position.x, particle.position.y, particle.position.z, 1.0); // 4. column
	

	//output the position of each vertex
	gl_Position = ViewProjectionMatrix.viewProj * aMat4 * vec4(positions[gl_VertexIndex], 1.0f);
	//outColor = colors[gl_VertexIndex];
	if(particle.color.x == 0) {
		outColor = vec3(1.0, 0.0, 0.0);
	} else {
		outColor = vec3(0.0, 1.0, 0.0);
	}
	float col = float(particle.color.x);
	vec3 hsl = vec3(col / 8.0f, 0.8, 0.8);
	//outColor = vec3(col.x & 1, col.x & 2, 1);
	outColor = hsl2rgb(hsl);

	pos = particle.position.xyz;
	localPos = (aMat4 * vec4(positions[gl_VertexIndex], 1.0)).rgb;
}