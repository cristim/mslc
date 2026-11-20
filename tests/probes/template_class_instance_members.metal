// EXPECT: valid
// DISASM-MATCH: OpFunctionCall %_struct_[0-9]+ %[_0-9a-zA-Z]+ %float_2[^_0-9]
// DISASM-NOT: OpFunction %int 
// Refs #341: a class template instance is an ordinary struct; its constructor and member functions are helpers named after it.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box {
	T v;
	Box(T x) : v(x) {}
	T get() const { return v; }
	T twice() const { return get() + get(); }
	static T zero() { return T(0); }
};
float helper(Box<float> b) { return b.twice(); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Box<float> b(2.0f);
	out[i] = b.get() + helper(b) + Box<float>::zero();
}
