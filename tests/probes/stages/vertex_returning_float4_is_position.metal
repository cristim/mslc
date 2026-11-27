// EXPECT: valid
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ BuiltIn Position
// DISASM-NOT: Location
// Apple takes a bare float4 from a vertex function as its position (MSL 5.2.3).
vertex float4 vertex_returning_float4_is_position() {
    return float4(0.0, 0.0, 0.0, 1.0);
}
