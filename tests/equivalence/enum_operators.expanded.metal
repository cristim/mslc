#include <metal_stdlib>
using namespace metal;

kernel void enum_operators(device int *out [[buffer(0)]])
{
    out[0] = 3;
    out[1] = 5;
    out[2] = 5;
    out[3] = -2;
    out[4] = 1;
    out[5] = 0;
    out[6] = 1;
    out[7] = 0;
    out[8] = 1;
    out[9] = 1;
    out[10] = 0;
    out[11] = 1;
    out[12] = 1;
    out[13] = 0;
    out[14] = 0;
    out[15] = 1;
    out[16] = 2;
    out[17] = 3;
    out[18] = 4;
    out[19] = 32;
    out[20] = -3;
    out[21] = -1;
    out[22] = 2;
    out[23] = 4;
    out[24] = -2;
    out[25] = 6;
    out[26] = 9;
}
