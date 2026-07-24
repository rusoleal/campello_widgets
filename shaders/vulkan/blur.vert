#version 450

layout(push_constant) uniform BlurUniforms {
    vec4  dstRect;      // x, y, w, h (pixels, destination quad)
    vec4  srcRect;      // u0, v0, u1, v1 (normalised UV of source region)
    vec2  viewport;     // framebuffer width, height
    float sigma;        // Gaussian sigma (pixels)
    float horizontal;   // 1.0 = horizontal pass, 0.0 = vertical pass
    vec2  tex_size;     // source texture width, height (pixels)
    vec2  _pad;
} u;

layout(location = 0)      out vec2  v_uv;
layout(location = 1) flat out float v_sigma;
layout(location = 2) flat out float v_horizontal;
layout(location = 3) flat out vec2  v_tex_size;

const vec2 kCorners[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
    vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0)
);

void main()
{
    vec2 t   = kCorners[gl_VertexIndex];
    vec2 px  = vec2(u.dstRect.x + t.x * u.dstRect.z,
                    u.dstRect.y + t.y * u.dstRect.w);
    // Standard Vulkan pixel→NDC (y=-1 top, y=+1 bottom).
    vec2 ndc = (px / u.viewport) * 2.0 - 1.0;
    gl_Position  = vec4(ndc, 0.0, 1.0);
    v_uv         = vec2(u.srcRect.x + t.x * (u.srcRect.z - u.srcRect.x),
                        u.srcRect.y + t.y * (u.srcRect.w - u.srcRect.y));
    v_sigma      = u.sigma;
    v_horizontal = u.horizontal;
    v_tex_size   = u.tex_size;
}
