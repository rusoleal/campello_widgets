// WGSL port of shaders/vulkan/vertices.vert (y-up clip space). Shares
// rect_layout_, so the push-constant block is RectUniforms-sized; only the
// leading viewport is read.
struct VerticesUniforms { viewport: vec2f, _pad: vec2f };
@group(0) @binding(0) var<uniform> u: VerticesUniforms;

struct VSOut { @builtin(position) pos: vec4f, @location(0) color: vec4f };

@vertex fn main(@location(0) posw: vec3f, @location(1) color: vec4f) -> VSOut {
    var out: VSOut;
    out.pos = vec4f(2.0 * posw.x / u.viewport.x - posw.z,
                    posw.z - 2.0 * posw.y / u.viewport.y, 0.0, posw.z);
    out.color = color;
    return out;
}
