// EXPECT: valid
// DISASM-MATCH: OpSelectionMerge
// A comma works in an if condition, in a ternary arm and in an index; in a call
// argument list a comma is still the separator.
float add(float x, float y) { return x + y; }
kernel void comma_in_condition_and_ternary(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  int n = 0, j = 0;
  if (j = 1, n == 0) {
    o[(j, 1)] = i > 0u ? (j++, 2.0f) : 3.0f;
  }
  o[0] = add(1.0f, 2.0f);
}
