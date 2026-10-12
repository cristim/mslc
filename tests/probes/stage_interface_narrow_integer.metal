// EXPECT: valid
// A 16-bit integer crosses a stage as a 32-bit one of the same signedness, which keeps
// its value without the storageInputOutput16 feature (lavapipe draw: -1000 and 60000 exact).
struct In {
    float4 p [[position]];
    short s;
};

fragment float4 stage_interface_narrow_integer(In in [[stage_in]])
{
    return float4(float(in.s));
}
