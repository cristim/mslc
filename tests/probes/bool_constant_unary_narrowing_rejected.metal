// EXPECT: error narrowing
//
// -true folds to the int -1, which does not fit a uchar; Apple rejects it the same way.
kernel void bool_constant_unary_narrowing_rejected(device uint *o [[buffer(0)]])
{
    uchar a{-true};
    o[0] = a;
}
