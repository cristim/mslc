// EXPECT: valid
// DISASM-MATCH: OpConstantComposite %v4bool %false(_[0-9]+)? %true(_[0-9]+)? %true(_[0-9]+)? %true(_[0-9]+)?[^0-9_]
// DISASM-MATCH: OpConstantComposite %v4bool %true(_[0-9]+)? %true(_[0-9]+)? %true(_[0-9]+)? %true(_[0-9]+)?[^0-9_]
// DISASM-MATCH: OpConstantComposite %v2bool %false(_[0-9]+)? %true(_[0-9]+)?[^0-9_]
//
// A bool vector converts each component as "bool(x)": nonzero is true, even a negative integer
// or a fraction.
constant bool4 kFromInts = bool4(0, 1, 2, -1);
constant bool4 kSplat = bool4(true);
constant bool2 kFloats = bool2(0.0f, 0.5f);

kernel void file_scope_constant_bool_vector_constructor(device int *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = int(kFromInts.x) + int(kSplat.w) + int(kFloats.y);
}
