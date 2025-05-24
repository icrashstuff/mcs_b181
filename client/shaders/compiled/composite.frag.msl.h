// clang-format off
static const unsigned char composite_frag_msl[] = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float4 out_color [[color(0)]];
};

fragment main0_out main0(texture2d<float> tex [[texture(0)]], sampler texSmplr [[sampler(0)]], float4 gl_FragCoord [[position]])
{
    main0_out out = {};
    out.out_color = tex.read(uint2(int2(gl_FragCoord.xy)), 0);
    return out;
}

)";
static const unsigned int composite_frag_msl_len = sizeof(composite_frag_msl);
