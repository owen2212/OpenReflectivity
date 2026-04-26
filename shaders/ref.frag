#version 330 core

in float v_gate;
out vec4 FragColor;

uniform sampler1D u_dbz_lut;
uniform float u_min_dbz;
uniform float u_max_dbz;

void main() {
    if (v_gate <= -9999.0) discard;
    if (v_gate < u_min_dbz) discard;

    float t = clamp((v_gate - u_min_dbz) / (u_max_dbz - u_min_dbz), 0.0, 1.0);
    vec3 color = texture(u_dbz_lut, t).rgb;
    FragColor = vec4(color, 1.0);
}
