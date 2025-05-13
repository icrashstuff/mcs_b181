// clang-format off
static const unsigned char background_vert_msl[] = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float2 fragcoords [[user(locn0)]];
    float4 gl_Position [[position]];
};

vertex main0_out main0(uint gl_VertexIndex [[vertex_id]])
{
    main0_out out = {};
    out.fragcoords = float2(float(int(gl_VertexIndex) & 1), float((int(gl_VertexIndex) >> 1) & 1));
    out.gl_Position = float4((out.fragcoords * 2.0) - float2(1.0), 0.0, 1.0);
    return out;
}

)";
static const unsigned int background_vert_msl_len = sizeof(background_vert_msl);
