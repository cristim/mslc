// EXPECT: valid
// RUN: grid=1 local=1 cases=1 elems=64
// A unary + - ~ on a bool in a constant expression gives an int, so a brace list narrows
// {+true} and {-false} to uint (1 and 0). Only constants are involved, so this is the folder
// and the narrowing proof, not the runtime lowering.
constant int kNeg = -true;
constant int kNot = ~true;
constant uint kPlus = +true;
kernel void bool_constant_unary_promotes(device int *oi [[buffer(0)]])
{
    constexpr bool t = true;
    constexpr bool f = false;
    uint a{+true};
    uint b{-false};
    constexpr int c = -t;
    constexpr int d = ~f;
    constexpr int e = +t + -t;
    oi[0] = int(a);
    oi[1] = int(b);
    oi[2] = c;
    oi[3] = d;
    oi[4] = e;
    oi[5] = kNeg;
    oi[6] = kNot;
    oi[7] = int(kPlus);
}
