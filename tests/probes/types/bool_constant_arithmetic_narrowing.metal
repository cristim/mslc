// EXPECT: valid
// RUN: grid=1 local=1 cases=1 elems=64
// A bool in a constant expression is an int: true + 1 is 2, -true is -1, ~true is -2, and the
// narrowing proof of a brace list accepts the result.
kernel void bool_constant_arithmetic_narrowing(device float *out [[buffer(0)]], device int *oi [[buffer(1)]])
{
    float a{true + 1};
    float b{true + true};
    int c{-true};
    int d{~true};
    int e{true - 3};
    char f{true * 100};
    constexpr bool t = true;
    constexpr int g = t + t * 5;
    float h{t + 0.5f};
    out[0] = a;
    out[1] = b;
    out[2] = h;
    oi[0] = c;
    oi[1] = d;
    oi[2] = e;
    oi[3] = f;
    oi[4] = g;
    oi[5] = true << 3;
    oi[6] = 7 % true + true / 2;
}
