// EXPECT: valid
// DISASM: OpTypeArray %float %uint_1032
// DISASM: OpTypeArray %float %uint_2032
// DISASM: OpTypeArray %float %uint_3001
// DISASM: OpTypeArray %float %uint_4012
// DISASM: OpTypeArray %float %uint_5016
// DISASM: OpTypeArray %float %uint_6002
// DISASM: OpTypeArray %float %uint_7016
// DISASM: OpTypeArray %float %uint_8048
// DISASM: OpTypeArray %float %uint_9048
// DISASM: OpTypeArray %float %uint_10016
// DISASM: OpTypeArray %float %uint_11016
// DISASM: OpTypeArray %float %uint_12032
// DISASM: OpTypeArray %float %uint_13008
// DISASM: OpTypeArray %float %uint_14064
// DISASM: OpTypeArray %float %uint_15036
//
// A struct is its members in order, each at a multiple of its own alignment, rounded up to the
// largest alignment: Gap is 4 + 4 + 1 padded to 12, Tail's float3 pads it to 32, and a nested struct
// and a pointer member take their own size and alignment.
// Each size is read back as the length of a distinct array (the size plus 1000 times its
// position), so a wrong size for any one type changes exactly one array. Apple's compiler is the
// oracle for every figure (xcrun metal -S -emit-llvm over the same sizeofs).
#include <metal_stdlib>
using namespace metal;
struct Pad { float a; float3 b; };
struct Tail { float3 a; float b; };
struct One { char a; };
struct Gap { char a; int b; char c; };
struct Arr { float a[3]; half h; };
struct Flags { bool a; bool b; };
struct Packed { packed_float3 p; float x; };
struct Mat { float2x3 m; char c; };
struct Nest { Pad a; char c; };
struct Wide { long l; char c; };
struct Ptr { device float *p; char c; };
struct Vecs { float3 a[2]; };
struct Half3 { half3 h; };
#define CHECK(k, T) float a##k[sizeof(T) + 1000 * k]; a##k[0] = 1.0f; sum += a##k[0];
kernel void sizeof_struct_layout(device float *out [[buffer(0)]])
{
    float sum = 0.0f;
    CHECK(1, Pad)
    CHECK(2, Tail)
    CHECK(3, One)
    CHECK(4, Gap)
    CHECK(5, Arr)
    CHECK(6, Flags)
    CHECK(7, Packed)
    CHECK(8, Mat)
    CHECK(9, Nest)
    CHECK(10, Wide)
    CHECK(11, Ptr)
    CHECK(12, Vecs)
    CHECK(13, Half3)
    CHECK(14, Pad[2])
    CHECK(15, Gap[3])
    out[0] = sum;
}
