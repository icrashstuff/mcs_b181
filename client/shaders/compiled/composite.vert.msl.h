// clang-format off
static const unsigned char composite_vert_msl[] = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float4 gl_Position [[position]];
};

vertex main0_out main0(uint gl_VertexIndex [[vertex_id]])
{
    main0_out out = {};
    out.gl_Position = float4((float2(float(int(gl_VertexIndex) & 1), float((int(gl_VertexIndex) >> 1) & 1)) * 2.0) - float2(1.0), 0.0, 1.0);
    return out;
}

)";
static const unsigned int composite_vert_msl_len = sizeof(composite_vert_msl);
