#version 150

out vec4 oColor;

uniform sampler2D 	uTex0;
uniform float u_scale;
uniform float u_gamma_correction;

in vec2 TexCoord0;

void main(void)
{
	vec4 color = texture( uTex0, TexCoord0 );
	oColor = pow(color * u_scale, vec4(u_gamma_correction));
	oColor.a = 1.0;
}
