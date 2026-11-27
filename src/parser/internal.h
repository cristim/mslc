#pragma once

#include <algorithm>
#include <climits>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "layout.h"
#include "parser.h"

namespace mslc {

// Thrown to stop a re-read of the file where a template's declaration ends.
struct StopParsing {};

constexpr const char* kStdlibNamespace = "metal";
constexpr const char* kAnonymousNamespace = "(anonymous namespace)";

// Names the typedef and enum support has to keep distinct from everything else.
constexpr char kAnonymousStructTypedef[] = "typedef of an anonymous struct";

std::string joinParts(const std::vector<std::string>& parts, size_t from, size_t to);

struct StdlibLookup {
	bool scopeExists = false;
	bool hasName = false;
};

StdlibLookup stdlibLookup(std::string_view scope, std::string_view name);
const std::unordered_map<std::string_view, ScalarKind>& scalarTypeNames();
bool isScalarTypeName(std::string_view text, ScalarKind& outKind);
bool isUnpackedTypeName(std::string_view text, Type& outType);
bool isPackedVectorName(std::string_view text, Type& outType);
bool isTypeName(std::string_view text, Type& outType);
bool isTypeName(std::string_view text);
bool isMetalBuiltinTypeName(std::string_view text);
bool isSimdVectorName(std::string_view text, Type& outType);
bool isIntegerSpecifier(std::string_view text);
bool isTypeQualifier(std::string_view text);
bool isResourceTypeName(std::string_view text);
std::optional<Interpolation> interpolationFromName(std::string_view name);
std::optional<ParameterAttributes::Builtin> builtinFromName(std::string_view name);
bool isCastTypeName(std::string_view name);
std::optional<AddressSpace> addressSpaceFor(std::string_view name);
void rejectLocalQualifiers(const Type& type, bool isReference, bool referenceAllowed = false);

bool tagBesideObject(std::string_view a, std::string_view b);
bool tagBesideNonTag(std::string_view a, std::string_view b);
void rejectUnloweredReturn(const Type& returned, const std::string& quoted, bool allowStdArray = false);
void takeReferenceReturn(FunctionDecl& decl, const std::string& quoted);
bool isResourcePointer(const Type& type);
Stage stageForKeyword(std::string_view word);

class ClassTemplateError: public CompileError {
public:
	using CompileError::CompileError;
};

class DependentClassInstance: public ClassTemplateError {
public:
	using ClassTemplateError::ClassTemplateError;
};

class DependentTrait: public CompileError {
public:
	using CompileError::CompileError;
};

class RestoreFlag {
	bool& _flag;
	bool _old;

public:
	RestoreFlag(bool& flag, bool value): _flag(flag), _old(flag) { _flag = value; }
	~RestoreFlag() { _flag = _old; }
};

bool isDeductionMarker(const Type& type);
bool isScalarOrVector(const Type& type);
bool sameTraitType(const Type& a, const Type& b);
bool traitHolds(const std::string& trait, const std::vector<Type>& operands, const TranslationUnit& unit);

bool sameType(const Type& a, const Type& b);
bool isTypeSupportKind(std::string_view what);
bool integerRange(ScalarKind kind, int64_t& low, int64_t& high);
void checkAttributeIndex(const std::string& name, uint32_t index);

inline ExpressionPtr makeIdentifier(const std::string& name, size_t line) {
	auto expression = std::make_unique<Expression>();
	expression->kind = ExpressionKind::Identifier;
	expression->line = line;
	expression->name = name;
	return expression;
}

// "this.field": the object a member function or constructor works on is a
// parameter or a local named this.
inline ExpressionPtr makeThisMember(const std::string& field, size_t line) {
	auto member = std::make_unique<Expression>();
	member->kind = ExpressionKind::Member;
	member->line = line;
	member->left = makeIdentifier("this", line);
	member->memberName = field;
	member->height = 2;
	return member;
}

} // namespace mslc
