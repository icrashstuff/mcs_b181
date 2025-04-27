// clang-format off
static const unsigned char terrain_vert_msl[] = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct vertex_t
{
    uint pos_ao;
    uint coloring;
    uint texturing;
};

struct vertex_data_t
{
    vertex_t data[1];
};

struct ubo_world_t
{
    float4x4 camera;
    float4x4 projection;
};

struct ubo_model_t
{
    float4 model;
};

struct _108
{
    float4 color;
    float2 uv;
    float ao;
    float light_block;
    float light_sky;
};

struct ubo_tint_t
{
    float4 tint;
};

struct main0_out
{
    float4 frag_color [[user(locn0)]];
    float2 frag_uv [[user(locn1)]];
    float frag_ao [[user(locn2)]];
    float frag_light_block [[user(locn3)]];
    float frag_light_sky [[user(locn4)]];
    float4 gl_Position [[position]];
};

vertex main0_out main0(constant ubo_world_t& ubo_world [[buffer(0)]], constant ubo_tint_t& ubo_tint [[buffer(1)]], constant ubo_model_t& ubo_model [[buffer(2)]], const device vertex_data_t& vtx_data [[buffer(3)]], uint gl_InstanceIndex [[instance_id]], uint gl_VertexIndex [[vertex_id]])
{
    main0_out out = {};
    _108 frag = {};
    int _24 = (int(gl_InstanceIndex) * 4) + int(gl_VertexIndex);
    out.gl_Position = (ubo_world.projection * ubo_world.camera) * (float4(float(int(vtx_data.data[_24].pos_ao & 1023u) - 256) * 0.03125, float(int((vtx_data.data[_24].pos_ao >> uint(10)) & 1023u) - 256) * 0.03125, float(int((vtx_data.data[_24].pos_ao >> uint(20)) & 1023u) - 256) * 0.03125, 1.0) + ubo_model.model);
    frag.ao = float((vtx_data.data[_24].pos_ao >> uint(30)) & 3u) * 0.3333333432674407958984375;
    frag.color.x = (ubo_tint.tint.x * float(vtx_data.data[_24].coloring & 255u)) * 0.0039215688593685626983642578125;
    frag.color.y = (ubo_tint.tint.y * float((vtx_data.data[_24].coloring >> uint(8)) & 255u)) * 0.0039215688593685626983642578125;
    frag.color.z = (ubo_tint.tint.z * float((vtx_data.data[_24].coloring >> uint(16)) & 255u)) * 0.0039215688593685626983642578125;
    frag.color.w = ubo_tint.tint.w;
    frag.light_block = float((vtx_data.data[_24].coloring >> uint(24)) & 15u) * 0.066666670143604278564453125;
    frag.light_sky = float((vtx_data.data[_24].coloring >> uint(28)) & 15u) * 0.066666670143604278564453125;
    frag.uv = float2(float(vtx_data.data[_24].texturing & 65535u) * 3.0517578125e-05, float((vtx_data.data[_24].texturing >> uint(16)) & 65535u) * 3.0517578125e-05);
    out.frag_color = frag.color;
    out.frag_uv = frag.uv;
    out.frag_ao = frag.ao;
    out.frag_light_block = frag.light_block;
    out.frag_light_sky = frag.light_sky;
    return out;
}

)";
static const unsigned int terrain_vert_msl_len = sizeof(terrain_vert_msl);
