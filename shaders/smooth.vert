#version 330 core

layout(location = 0) in vec2 in_pos;  // [-1,1] x [-1,1]

uniform float u_max_range;
uniform vec2 u_view_scale;
uniform vec2 u_view_offset;

out vec2 v_world;

void main() {
    v_world = in_pos * u_max_range;
    vec2 ndc = v_world * u_view_scale + u_view_offset;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
