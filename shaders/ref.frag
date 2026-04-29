#version 330 core

in float v_gate;
out vec4 FragColor;

uniform sampler1D u_value_lut;
uniform float u_min_value;
uniform float u_max_value;
uniform float u_discard_below;

void main() {
    if (v_gate <= -9999.0) discard;
    if (v_gate < u_discard_below) discard;

    float t = clamp((v_gate - u_min_value) / (u_max_value - u_min_value), 0.0, 1.0);
    vec3 color = texture(u_value_lut, t).rgb;
    FragColor = vec4(color, 1.0);
}
