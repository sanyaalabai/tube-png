#version 330 core
out vec4 frag_COLOR;
in vec2 frag_UV;

struct Material {
	sampler2D diffuse0;
};
uniform Material material;

void main() {
	vec4 tex = texture(material.diffuse0, frag_UV);
	if(tex.a < 0.1) discard;
	frag_COLOR = tex;
}