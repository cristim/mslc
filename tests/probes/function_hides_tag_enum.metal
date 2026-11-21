// EXPECT: valid
// Apple calls the function; the enum cast would be a ConvertSToF of 1.
// DISASM-MATCH: OpFunctionCall %int
// DISASM: %int_100
enum S { A, B };
int S(int c) { return c + 100; }
kernel void function_hides_tag_enum(device float* out [[buffer(0)]]) { out[0] = float(S(1)); }
