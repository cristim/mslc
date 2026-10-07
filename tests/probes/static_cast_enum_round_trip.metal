// EXPECT: valid
// DISASM: = OpConvertSToF %float
//
// An enum is its underlying int, as in the C-style cast probe
// cast_enum_and_typedef_names, so the two inner casts lower to no instruction;
// the needle pins the outer float conversion and the parse of the nesting.
enum Mode { kA, kB };
kernel void static_cast_enum_round_trip(device float* out [[buffer(0)]], constant int* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    Mode m = static_cast<Mode>(in[i]);
    out[i] = static_cast<float>(static_cast<int>(m));
}
