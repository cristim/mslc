// A scalar of one type in an operation with a scalar of another, which is what
// MSL's usual arithmetic conversions resolve rather than mslc's own.
//
// Every expression here is one a Metal shader writes without thinking: an
// integer literal against a float, and a float against one. SPIR-V's binary
// arithmetic is typed on both sides being the same type and converts neither,
// so each of these needs the integer widened to the float before the operation
// has an instruction at all.
//
// The values are chosen so that getting the conversion the wrong way round
// cannot hide. Reading "x > 0" as an unsigned comparison puts a negative float
// on the greater side of zero, and 1.0 / x is 1/8 as often as 1/1, so an
// integer reading of either is a different number rather than a rounded one.
// The vector case is here for the same reason: "values[0] * 2" is a float4 times
// a float4 once the literal is a float, and was a shape mismatch before.
struct Composed
{
    float4 below;
    float4 above;
    float4 arithmetic;
    float4 reverse;
    uint belowIsGreater;
    uint aboveIsGreater;
};

kernel void promote(device Composed *out [[buffer(0)]],
                    constant float4 *values [[buffer(1)]],
                    uint index [[thread_position_in_grid]])
{
    float below = values[0].x;
    float above = values[0].y;

    // A comparison against a widened zero, read through a branch so what is
    // compared decides what is written. Below zero, so an unsigned reading of
    // the same constant would take the other branch.
    if (below > 0) {
        out[index].belowIsGreater = 1;
    } else {
        out[index].belowIsGreater = 0;
    }

    if (above > 0) {
        out[index].aboveIsGreater = 1;
    } else {
        out[index].aboveIsGreater = 0;
    }

    // An integer literal as the right-hand side of an arithmetic operation, on a
    // scalar and on a vector.
    out[index].arithmetic = float4(below * 2, above / 4, above - 1, below + 3);
    out[index].reverse = values[0] * 2 + values[1] / 4;

    // A float constant as the left-hand side, so the conversion is the other way
    // round: the literal widens, not the value.
    out[index].below = float4(0.5 * below, 0.25 * above, 2.0 + below, 1.0 / above);
}
