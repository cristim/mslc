// EXPECT: error macro expansion holds more than
// F(F(F(...))) doubles at every level; the limit stops it before memory does.
#define F(x) x x
uint a = F(F(F(F(F(F(F(F(F(F(F(F(F(F(F(F(F(F(F(F(F(F(F(F(F(F(F(F(F(F(1))))))))))))))))))))))))))))));
