// clang-format off
static const unsigned char clouds_vert_msl[] = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct ubo_cloud_vert_t
{
    float4 camera_pos;
    float height;
    float render_distance;
};

struct ubo_world_t
{
    float4x4 camera;
    float4x4 projection;
};

struct main0_out
{
    float3 out_camera_pos [[user(locn0)]];
    float3 out_cloud_pos [[user(locn1)]];
    float4 gl_Position [[position]];
};

vertex main0_out main0(constant ubo_world_t& ubo_world [[buffer(0)]], constant ubo_cloud_vert_t& ubo_cloud_vert [[buffer(1)]], uint gl_VertexIndex [[vertex_id]])
{
    main0_out out = {};
    float _22 = ubo_cloud_vert.render_distance * 25.6000003814697265625;
    float3 _30 = float3(_22, ubo_cloud_vert.height, _22);
    float3 _112;
    if (extract_bits(int(gl_VertexIndex), uint(0), uint(1)) != int(0u))
    {
        float3 _104 = _30;
        _104.x = ubo_cloud_vert.render_distance * (-25.6000003814697265625);
        _112 = _104;
    }
    else
    {
        _112 = _30;
    }
    float3 _113;
    if (extract_bits(int(gl_VertexIndex), uint(1), uint(1)) != int(0u))
    {
        float3 _107 = _112;
        _107.z = _112.z * (-1.0);
        _113 = _107;
    }
    else
    {
        _113 = _112;
    }
    float2 _65 = _113.xz + ubo_cloud_vert.camera_pos.xz;
    float _67 = _65.x;
    float3 _109 = _113;
    _109.x = _67;
    float _69 = _65.y;
    _109.z = _69;
    out.out_cloud_pos = _109;
    out.out_camera_pos = ubo_cloud_vert.camera_pos.xyz;
    out.gl_Position = (ubo_world.projection * ubo_world.camera) * float4(_67, _113.y, _69, 1.0);
    return out;
}

)";
static const unsigned int clouds_vert_msl_len = sizeof(clouds_vert_msl);
