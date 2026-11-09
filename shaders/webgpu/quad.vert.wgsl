// WGSL port of shaders/vulkan/quad.vert (y-up clip space). The layout has
// one bind group (the texture), so the push constants are in group 1.
struct QuadUniforms { viewport: vec2f, opacity: f32, _pad: f32 };
@group(1) @binding(0) var<uniform> u: QuadUniforms;

struct VSOut { @builtin(position) pos: vec4f, @location(0) uv: vec2f, @location(1) opacity: f32 };

@vertex fn main(@location(0) posw: vec3f, @location(1) uv: vec2f) -> VSOut {
    var out: VSOut;
    out.pos = vec4f(2.0 * posw.x / u.viewport.x - posw.z,
                    posw.z - 2.0 * posw.y / u.viewport.y, 0.0, posw.z);
    out.uv = uv;
    out.opacity = u.opacity;
    return out;
}
