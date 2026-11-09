// WGSL port of shaders/vulkan/vertices.frag.
@fragment fn main(@location(0) color: vec4f) -> @location(0) vec4f {
    return vec4f(color.rgb * color.a, color.a);
}
