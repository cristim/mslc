// EXPECT: error more than 64 class template instances
// Refs #341: slice 5d: each instance named only by function templates costs one more parse of the file, so the number is capped.
#include <metal_stdlib>
using namespace metal;
template <int N> struct B { int v; int get() const { return v + N; } };
template <int N> int f() { B<N> b; b.v = 1; return b.get(); }
kernel void k(device int* out [[buffer(0)]])
{
	int s = 0;
	s += f<0>();
	s += f<1>();
	s += f<2>();
	s += f<3>();
	s += f<4>();
	s += f<5>();
	s += f<6>();
	s += f<7>();
	s += f<8>();
	s += f<9>();
	s += f<10>();
	s += f<11>();
	s += f<12>();
	s += f<13>();
	s += f<14>();
	s += f<15>();
	s += f<16>();
	s += f<17>();
	s += f<18>();
	s += f<19>();
	s += f<20>();
	s += f<21>();
	s += f<22>();
	s += f<23>();
	s += f<24>();
	s += f<25>();
	s += f<26>();
	s += f<27>();
	s += f<28>();
	s += f<29>();
	s += f<30>();
	s += f<31>();
	s += f<32>();
	s += f<33>();
	s += f<34>();
	s += f<35>();
	s += f<36>();
	s += f<37>();
	s += f<38>();
	s += f<39>();
	s += f<40>();
	s += f<41>();
	s += f<42>();
	s += f<43>();
	s += f<44>();
	s += f<45>();
	s += f<46>();
	s += f<47>();
	s += f<48>();
	s += f<49>();
	s += f<50>();
	s += f<51>();
	s += f<52>();
	s += f<53>();
	s += f<54>();
	s += f<55>();
	s += f<56>();
	s += f<57>();
	s += f<58>();
	s += f<59>();
	s += f<60>();
	s += f<61>();
	s += f<62>();
	s += f<63>();
	s += f<64>();
	s += f<65>();
	out[0] = s;
}
