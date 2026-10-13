// EXPECT: valid
// DISASM: OpKill
// "using metal::discard_fragment;" in a function body, then the bare call discards.
#include <metal_stdlib>
fragment float4 using_declaration_of_discard_fragment_in_block(float4 position [[position]])
{
    if (position.x < 1.0) {
        using metal::discard_fragment;
        discard_fragment();
    }
    return float4(1.0);
}
