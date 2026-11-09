// WGSL port of shaders/vulkan/clip_shape.frag.
@group(0) @binding(1) var u_texture: texture_2d<f32>;
@group(0) @binding(2) var u_sampler: sampler;

@fragment fn main(@location(0) uv: vec2f,
                  @location(1) @interpolate(flat) rect_size: vec2f,
                  @location(2) @interpolate(flat) corner_r: f32,
                  @location(3) @interpolate(flat) kind: f32) -> @location(0) vec4f {
    let child_color = textureSample(u_texture, u_sampler, uv);
    let hs = rect_size * 0.5;
    let p = (uv - vec2f(0.5)) * rect_size;
    var d: f32;
    if (kind < 0.5) {
        let r = min(corner_r, min(hs.x, hs.y));
        let q = abs(p) - hs + r;
        d = length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0) - r;
    } else {
        d = (length(p / hs) - 1.0) * min(hs.x, hs.y);
    }
    let alpha = 1.0 - smoothstep(-0.5, 0.5, d);
    return child_color * alpha;
}
