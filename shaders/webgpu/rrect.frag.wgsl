// WGSL port of shaders/vulkan/rrect.frag.
struct RRectUniforms { rect: vec4f, color: vec4f, viewport: vec2f, radius: f32, stroke_w: f32 };
@group(0) @binding(0) var<uniform> u: RRectUniforms;

fn roundedBox(p: vec2f, b: vec2f, r: f32) -> f32 {
    let q = abs(p) - b + r;
    return length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

@fragment fn main(@location(0) px: vec2f) -> @location(0) vec4f {
    let center = u.rect.xy + u.rect.zw * 0.5;
    let half_size = u.rect.zw * 0.5;
    let r = clamp(u.radius, 0.0, min(half_size.x, half_size.y));
    let d = roundedBox(px - center, half_size, r);

    var alpha: f32;
    if (u.stroke_w > 0.0) {
        let outer = 1.0 - smoothstep(-0.5, 0.5, d);
        let inner = 1.0 - smoothstep(-0.5, 0.5, d + u.stroke_w);
        alpha = outer - inner;
    } else {
        alpha = 1.0 - smoothstep(-0.5, 0.5, d);
    }
    // Zero-coverage fragments must not reach the blend unit (non-srcOver modes).
    if (alpha <= 0.0) { discard; }
    let c = u.color;
    return vec4f(c.rgb * c.a, c.a) * alpha;
}
