// WGSL port of shaders/vulkan/rect_aa.frag.
@fragment fn main(@location(0) color: vec4f, @location(1) alpha: f32) -> @location(0) vec4f {
    let a = color.a * alpha;
    return vec4f(color.rgb * a, a);
}
