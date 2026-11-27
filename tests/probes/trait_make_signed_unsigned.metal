// EXPECT: valid
// QUIRK (make_signed, make_unsigned): each assertion below holds on Apple's compiler (metal 32023.883, macOS 26).
// An integer maps to the other signedness at the same width, element-wise in a vector or packed vector; const
// and volatile are dropped, where C++ keeps them; make_unsigned keeps bool and make_signed keeps a
// floating-point type, where C++ refuses both.
#include <metal_stdlib>
using namespace metal;
static_assert(is_same_v<make_unsigned_t<int>, uint>, "");
static_assert(is_same_v<make_unsigned_t<uint>, uint>, "");
static_assert(is_same_v<make_unsigned_t<short>, ushort>, "");
static_assert(is_same_v<make_unsigned_t<char>, uchar>, "");
static_assert(is_same_v<make_unsigned_t<long>, ulong>, "");
static_assert(is_same_v<make_unsigned_t<int2>, uint2>, "");
static_assert(is_same_v<make_unsigned_t<short4>, ushort4>, "");
static_assert(is_same_v<make_unsigned_t<packed_int3>, packed_uint3>, "");
static_assert(is_same_v<make_unsigned_t<const int>, uint>, "");
static_assert(is_same_v<make_unsigned_t<volatile int>, uint>, "");
static_assert(is_same_v<make_unsigned_t<bool>, bool>, "");
static_assert(is_same_v<make_unsigned<int>::type, uint>, "");
static_assert(is_same_v<make_signed_t<uint>, int>, "");
static_assert(is_same_v<make_signed_t<uint3>, int3>, "");
static_assert(is_same_v<make_signed_t<ushort>, short>, "");
static_assert(is_same_v<make_signed_t<ulong2>, long2>, "");
static_assert(is_same_v<make_signed_t<int>, int>, "");
static_assert(is_same_v<make_signed_t<const uint2>, int2>, "");
static_assert(is_same_v<make_signed_t<half>, half>, "");
static_assert(is_same_v<make_signed_t<float2>, float2>, "");
static_assert(is_same_v<metal::make_signed<uint>::type, int>, "");
kernel void trait_make_signed_unsigned(device uint* out [[buffer(0)]]) { out[0] = 1; }
