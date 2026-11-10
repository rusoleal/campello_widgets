// WGSL port of shaders/vulkan/line.frag.
@fragment fn main(@location(0) color: vec4f, @location(1) px: vec2f, @location(2) p1: vec2f,
                  @location(3) p2: vec2f, @location(4) half_w: f32) -> @location(0) vec4f {
    let dir = p2 - p1;
    let len = length(dir);
    var dir_n = vec2f(1.0, 0.0);
    if (len > 0.0001) { dir_n = dir / len; }
    let perp_n = vec2f(-dir_n.y, dir_n.x);

    let rel = px - (p1 + p2) * 0.5;
    let along = dot(rel, dir_n);
    let perp = dot(rel, perp_n);

    let q = vec2f(abs(along) - len * 0.5, abs(perp) - half_w);
    let d = length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0);
    let alpha = 1.0 - smoothstep(-0.5, 0.5, d);
    if (alpha <= 0.0) { discard; }
    return vec4f(color.rgb * color.a, color.a) * alpha;
}
