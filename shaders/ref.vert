#version 330 core

layout(location = 0) in vec2 in_pos;   // unit quad parametric coord, [0,1] x [0,1]
layout(location = 1) in float gate;    // gate value
layout(location = 2) in int gate_idx;  // gate index along radial
layout(location = 3) in int radial_idx;// radial index

uniform samplerBuffer u_radial_meta;
uniform vec2 u_view_scale;
uniform vec2 u_view_offset;
uniform vec2 u_storm_motion;  // (east, north) m/s; (0,0) when not velocity / disabled

out float v_gate;

void main() {
    vec4 m = texelFetch(u_radial_meta, radial_idx);
    float az_start = m.x;
    float range_bin1 = m.y;
    float gate_size = m.z;
    float delta_az = m.w;

    // polar trapezoid corner: in_pos.x sweeps az_start -> az_start+delta_az,
    // in_pos.y sweeps inner -> outer range of this gate cell
    float az = az_start + in_pos.x * delta_az;
    float r  = range_bin1 + (float(gate_idx) + in_pos.y) * gate_size;

    // north-up, azimuth clockwise from north
    vec2 world = vec2(sin(az), cos(az)) * r;

    vec2 ndc = world * u_view_scale + u_view_offset;
    gl_Position = vec4(ndc, 0.0, 1.0);

    // storm-relative: subtract the storm motion's projection onto this
    // radial's direction (wedge center). Don't shift the sentinel.
    float az_c = az_start + 0.5 * delta_az;
    float corr = dot(vec2(sin(az_c), cos(az_c)), u_storm_motion);
    v_gate = (gate <= -9000.0) ? gate : gate - corr;
}
