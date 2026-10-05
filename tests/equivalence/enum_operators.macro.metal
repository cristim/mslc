#include <metal_stdlib>
using namespace metal;

enum Ops { O0 = 7 & 3, O1 = 4 | 1, O2 = 6 ^ 3, O3 = ~1, O4 = !0, O5 = !5, O6 = 3 < 4, O7 = 4 <= 3, O8 = 4 <= 4, O9 = 5 > 4, O10 = 4 >= 5, O11 = 5 >= 5, O12 = 2 == 2, O13 = 2 != 2, O14 = 1 && 0, O15 = 0 || 3, O16 = 17 % 5, O17 = 17 / 5, O18 = 16 >> 2, O19 = 1 << 5, O20 = -7 / 2, O21 = -7 % 3, O22 = +4, O23 = 3 - 5, O24 = 2 * 3, O25 = (1 + 2) * 3 };

kernel void enum_operators(device int *out [[buffer(0)]])
{
    out[0] = O0;
    out[1] = O1;
    out[2] = O2;
    out[3] = O3;
    out[4] = O4;
    out[5] = O5;
    out[6] = O6;
    out[7] = O7;
    out[8] = O8;
    out[9] = O9;
    out[10] = O10;
    out[11] = O11;
    out[12] = O12;
    out[13] = O13;
    out[14] = O14;
    out[15] = O15;
    out[16] = O16;
    out[17] = O17;
    out[18] = O18;
    out[19] = O19;
    out[20] = O20;
    out[21] = O21;
    out[22] = O22;
    out[23] = O23;
    out[24] = O24;
    out[25] = O25;
}
