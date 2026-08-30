// ===========================================================================
// campello_widgets DirectX 12 shaders — vertices pipeline
//
// Canvas.drawVertices() -- an arbitrary triangle mesh where each vertex
// carries its own full RGBA color (already paint-blended on the CPU, see
// Canvas::drawVertices()'s doc comment), linearly interpolated across each
// triangle. Structurally identical to rect_aa.hlsl minus its single-shared-
// uniform-color specialization: no uniform color at all, since color is
// entirely a per-vertex attribute -- only the viewport NDC-conversion
// constant rides the (reused) RectUniforms cbuffer, at the same offset its
// own `viewport` field already occupies.
//
// Bindings: RectUniforms — register(b0), vertex stage (reused, not a
// dedicated cbuffer — see D3DDrawBackend::vertices_pipeline_'s doc comment).
// ===========================================================================

cbuffer RectUniforms : register(b0)
{
    float4 color_UNUSED;  // occupies offset 0 so layout matches RectUniforms
    float2 viewport;      // width, height (pixels)
    float2 _pad;
};

struct VerticesVertexIn
{
    float3 posw  : TEXCOORD0;  // (x, y, w) -- CPU-transformed clip-space position
    float4 color : TEXCOORD1;  // (r, g, b, a) -- straight alpha
};

struct VerticesVertOut
{
    float4 pos   : SV_POSITION;
    float4 color : COLOR0;
};

VerticesVertOut VerticesVS(VerticesVertexIn input)
{
    float clip_x =  2.0 * input.posw.x / viewport.x - input.posw.z;
    float clip_y = -(2.0 * input.posw.y / viewport.y - input.posw.z);

    VerticesVertOut o;
    o.pos   = float4(clip_x, clip_y, 0.0, input.posw.z);
    o.color = input.color;
    return o;
}

float4 VerticesPS(VerticesVertOut input) : SV_TARGET
{
    float4 c = input.color;
    return float4(c.rgb * c.a, c.a);
}
