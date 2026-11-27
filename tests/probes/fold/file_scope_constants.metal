// EXPECT: valid
// DISASM: OpConstant %float 0.666851342
//
// A file-scope "constant" in the three forms a shader writes one:
//
//   constant float kEtaAir = 1.000277;           a literal
//   constant float kEtaRatio = kEtaAir / ...;     folding across declarations
//   constant Light light = { .direction = ... };  a struct, by field name
//
// SPIR-V takes only constants as a constant's operands, so kEtaRatio has to be
// folded before it is declared rather than emitted as the division it was
// written as. The DISASM line is the check: 1.000277f / 1.5f is 0.666851342 as
// the float nearest it, so a module that emitted the division rather than the
// value would not declare that constant at all.
//
// The struct is scalar-valued on purpose. A file-scope constant is the only
// thing here that needs a struct *as a value*, which is a different SPIR-V type
// from the same struct as a buffer's block: a Block-decorated struct is only
// valid in the storage classes a descriptor is allowed in, and using one as a
// constant's type is rejected.
constant float kEtaAir = 1.000277;
constant float kEtaGlass = 1.5;
constant float kEtaRatio = kEtaAir / kEtaGlass;

struct Light
{
    float direction;
    float ambient;
};

constant Light light = {
    .direction = 0.13,
    .ambient = 0.05
};

kernel void file_scope_constants(device float* out [[buffer(0)]],
                                 uint index [[thread_position_in_grid]])
{
    out[index] = light.ambient * kEtaRatio;
}
