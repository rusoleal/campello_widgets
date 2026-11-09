// WGSL port of shaders/vulkan/arc.frag. Angles are in screen space (y-down
// pixels), as in the GLSL: only the clip-space output was flipped.
struct ArcUniforms {
    rect: vec4f, color: vec4f, viewport: vec2f,
    stroke_w: f32, start_angle: f32, sweep_angle: f32,
};
@group(0) @binding(0) var<uniform> u: ArcUniforms;

const kPi = 3.14159265358979323846;

fn sdWedgeHalfPlanes(p: vec2f, mid: f32, half_angle: f32) -> f32 {
    let theta1 = mid - half_angle;
    let theta2 = mid + half_angle;
    let n1 = vec2f(-sin(theta1), cos(theta1));
    let n2 = vec2f(sin(theta2), -cos(theta2));
    return max(-dot(p, n1), -dot(p, n2));
}

@fragment fn main(@location(0) px: vec2f) -> @location(0) vec4f {
    let center = u.rect.xy + u.rect.zw * 0.5;
    let p = px - center;
    let hs = u.rect.zw * 0.5;

    let s = p / hs;
    let d_radial_fill = (length(s) - 1.0) * min(hs.x, hs.y);
    var d_radial = d_radial_fill;
    if (u.stroke_w > 0.0) { d_radial = max(d_radial_fill, -(d_radial_fill + u.stroke_w)); }

    let mid = u.start_angle + u.sweep_angle * 0.5;
    let half_angle = abs(u.sweep_angle) * 0.5;
    var d_angle: f32;
    if (half_angle <= kPi * 0.5) {
        d_angle = sdWedgeHalfPlanes(p, mid, half_angle);
    } else {
        d_angle = -sdWedgeHalfPlanes(p, mid + kPi, kPi - half_angle);
    }

    let d = max(d_radial, d_angle);
    let alpha = 1.0 - smoothstep(-0.5, 0.5, d);
    if (alpha <= 0.0) { discard; }
    let c = u.color;
    return vec4f(c.rgb * c.a, c.a) * alpha;
}
