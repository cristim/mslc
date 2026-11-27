// EXPECT: error "discard_fragment" is not a builtin mslc recognises
//
// A local named discard_fragment hides the <metal_stdlib> builtin, so calling it
// is a call on a float, never an OpKill. Apple rejects it too ("called object
// type 'float' is not a function or function pointer").
#include <metal_stdlib>
using namespace metal;
fragment float4 local_named_discard_fragment_call_rejected() {
	float discard_fragment = 1;
	discard_fragment();
	return 0;
}
