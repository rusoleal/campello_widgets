// WGSL port of shaders/vulkan/colored_quad.vert (y-up clip space).
struct ColoredQuadUniforms { rect: vec4f, color: vec4f, viewport: vec2f, _pad: vec2f };
@group(0) @binding(0) var<uniform> u: ColoredQuadUniforms;

struct VSOut { @builtin(position) pos: vec4f, @location(0) color: vec4f };

@vertex fn main(@location(0) posw: vec3f) -> VSOut {
    var out: VSOut;
    out.pos = vec4f(2.0 * posw.x / u.viewport.x - posw.z,
                    posw.z - 2.0 * posw.y / u.viewport.y, 0.0, posw.z);
    out.color = u.color;
    return out;
}
