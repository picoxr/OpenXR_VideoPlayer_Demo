#version 450

precision highp float;

layout(binding = 1) uniform sampler2D texSamplery;
layout(binding = 2) uniform sampler2D texSampleru;
layout(binding = 3) uniform sampler2D texSamplerv;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

const vec3 delta3  = vec3(1.0 / 12.92);
const vec3 alpha3  = vec3(1.0 / 1.055);
const vec3 theta3  = vec3(0.04045);
const vec3 offset3 = vec3(0.055);
const vec3 gamma3  = vec3(2.4);
// conversion based on: https://www.khronos.org/registry/DataFormat/specs/1.3/dataformat.1.3.html#TRANSFER_SRGB
vec4 sRGBToLinearRGB(vec4 srgba)
{
    const vec3 srgb = srgba.rgb;
    const vec3 lower = srgb * delta3;
    const vec3 upper = pow((srgb + offset3) * alpha3, gamma3);
    return vec4(mix(upper, lower, lessThan(srgb, theta3)), srgba.a);
}

void main() {
	vec3 yuv;
	vec3 rgb;
	yuv.r = texture(texSamplery, fragTexCoord).r;
	yuv.g = texture(texSampleru, fragTexCoord).r - 0.5;
	yuv.b = texture(texSamplerv, fragTexCoord).r - 0.5;
    rgb = mat3 (1.0,      1.0,      1.0,
                0.0,     -0.21482,  2.12798,
                1.28033, -0.38059,  0.0) * yuv;

	outColor = sRGBToLinearRGB(vec4(rgb, 1));
}
