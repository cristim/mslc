// EXPECT: valid
// RUN: grid=1 local=1
// DISASM-MATCH: OpFAdd
// Each file-scope declarator takes its own initialiser and sees the ones before it;
// the qualifiers are shared. clang C++ reference semantics (no Apple toolchain here).
constant float kA = 1.0, kB = kA + 1.0, kC = 3.0;
constant int kN = 2, kM = kN * 2;
constant float kTable[2] = {1.0, 2.0}, kScale = 4.0;
kernel void several_declarators_file_scope(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  o[0] = kA + kB + kC + float(kM) + kTable[i & 1] + kScale;
}
