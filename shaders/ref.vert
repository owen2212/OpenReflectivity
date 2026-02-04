#version 330 core

layout(location = 0) in vec2 in_pos;   // unit quad vertex
layout(location = 1) in float gate;    // gate value
layout(location = 2) in int gate_idx;  // gate index along radial
layout(location = 3) in int radial_idx;// radial index

uniform samplerBuffer u_radial_meta;
uniform vec2 u_view_scale;
uniform vec2 u_view_offset;

out float v_gate;

void main() {
    vec4 m = texelFetch(u_radial_meta, radial_idx);
    float azimuth_deg = m.x;
    float range_bin1 = m.y;
    float gate_size = m.z;

    float delta_az = m.w;
    float range_center = range_bin1 + gate_size * (float(gate_idx) + 0.5);
    float az = radians(azimuth_deg);

    float cell_height = gate_size;
    float cell_width = range_center * delta_az;

    vec2 radial_center = vec2(cos(az), sin(az)) * range_center;
    vec2 local = in_pos * vec2(cell_width, cell_height);
    mat2 rot = mat2(cos(az), -sin(az), sin(az), cos(az));
    vec2 cell_pos = radial_center + rot * local;

    vec2 ndc = cell_pos * u_view_scale + u_view_offset;
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_gate = gate;
}
