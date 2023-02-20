#version 450

layout(binding = 1) uniform sampler2D texSamplery;
layout(binding = 2) uniform sampler2D texSampleru;
layout(binding = 3) uniform sampler2D texSamplerv;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
	vec3 yuv;
	vec3 rgb;
	yuv.r = texture(texSamplery, fragTexCoord).r;
	yuv.g = texture(texSampleru, fragTexCoord).r - 0.5;
	yuv.b = texture(texSamplerv, fragTexCoord).r - 0.5;
	rgb = mat3 (1.0,     1.0,      1.0,
				0.0,     0.39465,  2.03211,
				1.13983, -0.5806,  0.0) * yuv;
	outColor = vec4(rgb, 1);
}
