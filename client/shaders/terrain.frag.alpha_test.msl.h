// clang-format off
static const unsigned char terrain_frag_alpha_test_msl[] = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct _11
{
    float4 color;
    float2 uv;
    float ao;
    float light_block;
    float light_sky;
};

struct ubo_frag_t
{
    uint use_texture;
};

struct main0_out
{
    float4 out_color [[color(0)]];
};

struct main0_in
{
    float4 frag_color [[user(locn0)]];
    float2 frag_uv [[user(locn1)]];
    float frag_ao [[user(locn2)]];
    float frag_light_block [[user(locn3)]];
    float frag_light_sky [[user(locn4)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant ubo_frag_t& ubo_frag [[buffer(0)]], texture2d<float> tex_atlas [[texture(0)]], texture2d<float> tex_lightmap [[texture(1)]], sampler tex_atlasSmplr [[sampler(0)]], sampler tex_lightmapSmplr [[sampler(1)]])
{
    main0_out out = {};
    _11 frag = {};
    frag.color = in.frag_color;
    frag.uv = in.frag_uv;
    frag.ao = in.frag_ao;
    frag.light_block = in.frag_light_block;
    frag.light_sky = in.frag_light_sky;
    out.out_color = frag.color;
    if (ubo_frag.use_texture == 1u)
    {
        out.out_color *= tex_atlas.sample(tex_atlasSmplr, frag.uv, bias(-1.0));
    }
    else
    {
        out.out_color.w *= tex_atlas.sample(tex_atlasSmplr, frag.uv, bias(-1.0)).w;
    }
    if (out.out_color.w < 0.0625)
    {
        discard_fragment();
    }
    float4 _81 = out.out_color;
    float3 _83 = _81.xyz * (1.0 - (frag.ao * 0.100000001490116119384765625));
    out.out_color.x = _83.x;
    out.out_color.y = _83.y;
    out.out_color.z = _83.z;
    float4 _102 = out.out_color;
    float3 _104 = _102.xyz * fast::clamp(1.10000002384185791015625 - ((-0.100000001490116119384765625) / (frag.ao - 1.25)), 0.0, 1.0);
    out.out_color.x = _104.x;
    out.out_color.y = _104.y;
    out.out_color.z = _104.z;
    out.out_color *= tex_lightmap.sample(tex_lightmapSmplr, float2(frag.light_block, frag.light_sky));
    return out;
}

)";
static const unsigned int terrain_frag_alpha_test_msl_len = sizeof(terrain_frag_alpha_test_msl);
