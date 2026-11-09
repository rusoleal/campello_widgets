// WGSL port of shaders/vulkan/shader_mask.frag. Gradient positions are in
// framebuffer pixels (origin top-left in both APIs).
@group(0) @binding(1) var u_child: texture_2d<f32>;
@group(0) @binding(2) var u_lut: texture_2d<f32>;
@group(0) @binding(3) var u_sampler: sampler;

const kPi = 3.14159265358979323846;

@fragment fn main(@builtin(position) frag_coord: vec4f,
                  @location(0) uv: vec2f,
                  @location(1) @interpolate(flat) gradient_type: f32,
                  @location(2) @interpolate(flat) gradient_p1: vec4f,
                  @location(3) @interpolate(flat) gradient_p2: vec4f,
                  @location(4) @interpolate(flat) blend_mode: f32,
                  @location(5) @interpolate(flat) tile_mode: f32) -> @location(0) vec4f {
    let child_color = textureSample(u_child, u_sampler, uv);
    let pos = frag_coord.xy;

    var t = 0.0;
    if (gradient_type < 0.5) {
        let dir = gradient_p2.xy - gradient_p1.xy;
        let len2 = dot(dir, dir);
        if (len2 > 0.0001) { t = dot(pos - gradient_p1.xy, dir) / len2; }
    } else if (gradient_type < 1.5) {
        let radius = gradient_p2.x;
        if (radius > 0.0001) { t = length(pos - gradient_p1.xy) / radius; }
    } else {
        let d = pos - gradient_p1.xy;
        var angle = atan2(d.y, d.x);
        if (angle < 0.0) { angle += 2.0 * kPi; }
        let span = gradient_p2.y - gradient_p2.x;
        if (abs(span) > 0.0001) { t = (angle - gradient_p2.x) / span; }
    }

    if (tile_mode < 0.5) {
        t = clamp(t, 0.0, 1.0);
    } else if (tile_mode < 1.5) {
        t = fract(t);
    } else {
        let period = t - 2.0 * floor(t * 0.5);
        t = select(period, 2.0 - period, period > 1.0);
    }

    let mask_color = textureSample(u_lut, u_sampler, vec2f(t, 0.5));
    if (blend_mode < 0.5) {
        return child_color * mask_color.a;
    }
    return child_color * mask_color;
}
