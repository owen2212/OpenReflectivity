#version 330 core

layout(location = 0) in vec2 in_pos; // [0,1] x [0,1]

uniform vec4 u_pixel_rect;   // x, y, w, h in pixels (origin = bottom-left)
uniform vec2 u_screen_size;  // viewport w, h in pixels

out vec2 v_uv;

void main() {
    vec2 px = u_pixel_rect.xy + in_pos * u_pixel_rect.zw;
    vec2 ndc = (px / u_screen_size) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_uv = in_pos;
}
