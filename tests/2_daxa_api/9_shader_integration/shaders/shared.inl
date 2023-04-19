#include <daxa/daxa.inl>
#include <daxa/utils/task_list.inl>

struct TestU64Alignment
{
    daxa_u32 i0;
    daxa_u32 i1;
    daxa_u32 i2;
    daxa_u64 i3;
    daxa_u32 i4;
    daxa_u64 i5[3];
    daxa_u32 i6[3];
    daxa_u64 i7;
};
DAXA_ENABLE_BUFFER_PTR(TestU64Alignment)

DAXA_INL_TASK_USES_BEGIN(TestShaderUses, DAXA_CBUFFER_SLOT0)
DAXA_INL_TASK_USE_BUFFER(align_test_src, daxa_BufferPtr(TestU64Alignment), COMPUTE_SHADER_READ)
DAXA_INL_TASK_USE_BUFFER(align_test_dst, daxa_RWBufferPtr(TestU64Alignment), COMPUTE_SHADER_READ_WRITE)
DAXA_INL_TASK_USES_END()

// Test compilation of shared functions with aligned types
daxa_u32 test_fn_u32(daxa_u32 by_value)
{
    return (by_value * by_value) * 4;
}

struct SomeStruct
{
    daxa_f32 value;
};
DAXA_ENABLE_BUFFER_PTR(SomeStruct)

struct RWHandles
{
    daxa_RWBufferPtr(SomeStruct) my_buffer;
    daxa_RWImage2Df32 my_image;
    daxa_SamplerId my_sampler;
};
DAXA_ENABLE_BUFFER_PTR(RWHandles)

struct BindlessTestPush
{
    RWHandles handles;
    daxa_RWBufferPtr(RWHandles) next_shader_input;
};

struct Handles
{
    daxa_BufferPtr(SomeStruct) my_buffer;
    daxa_Image2Df32 my_image;
    daxa_SamplerId my_sampler;
};
DAXA_ENABLE_BUFFER_PTR(Handles)

struct BindlessTestFollowPush
{
    daxa_BufferPtr(Handles) shader_input;
};