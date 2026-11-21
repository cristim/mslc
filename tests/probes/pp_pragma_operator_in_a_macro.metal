// EXPECT: valid
// DISASM: OpLoopMerge
// _Pragma("...") is the operator spelling of #pragma and is dropped the same way, wherever
// it is expanded: in a macro body (the ggml FOR_UNROLL idiom) and written directly.
#define FOR_UNROLL(x) _Pragma("clang loop unroll(full)") for (x)
kernel void pp_pragma_operator_in_a_macro(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
  uint s = 0;
  FOR_UNROLL(uint k = 0; k < 4; ++k) { s += k * i; }
  _Pragma("unroll") for (uint k = 0; k < 2; ++k) { s += k; }
  _Pragma("not a known pragma")
  out[i] = s;
}
