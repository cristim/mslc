// EXPECT: error fallthrough annotation does not directly precede switch label
// Apple does not treat discard_fragment() as leaving the block, so the marker is checked.
using namespace metal;
fragment float4 fallthrough_after_discard_fragment_rejected(float4 p [[position]]) {
  uint x = 0u;
  uint y = uint(p.x);
  uint i = y;
  switch (i) {
    case 0: discard_fragment(); [[fallthrough]]; x = 1u;
    case 9: x = 2u; break;
    default: break;
  }
  return float4(float(x));
}
