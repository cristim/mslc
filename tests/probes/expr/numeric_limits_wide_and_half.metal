// EXPECT: valid
// RUN: grid=1 local=1
// The 64-bit and half limits, with C++'s values: long min and max and ulong max as their bit patterns, and half's
// min, lowest, epsilon and infinity as theirs (0x0400, 0xFBFF, 0x1400, 0x7C00). On an RTX 3060 out is
// 1 1 1 1024 64511 5120 31744.
#include <metal_stdlib>
using namespace metal;
kernel void numeric_limits_wide_and_half(device int* out [[buffer(0)]]) {
    out[0] = numeric_limits<long>::min() == -9223372036854775807L - 1 ? 1 : 0;
    out[1] = numeric_limits<long>::max() == 9223372036854775807L ? 1 : 0;
    out[2] = numeric_limits<ulong>::max() == 18446744073709551615UL ? 1 : 0;
    out[3] = int(as_type<ushort>(numeric_limits<half>::min()));
    out[4] = int(as_type<ushort>(numeric_limits<half>::lowest()));
    out[5] = int(as_type<ushort>(numeric_limits<half>::epsilon()));
    out[6] = int(as_type<ushort>(numeric_limits<half>::infinity()));
}
