#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "layout.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"

namespace mslc {

using spirv::Id;
using spirv::InvalidId;

struct ScalarMapping {
	bool supported;
	bool isFloat;
	uint32_t width;
};

ScalarMapping mappingFor(ScalarKind kind);
bool isSignedInteger(ScalarKind kind);
std::optional<ScalarKind> integerKind(uint32_t bits, bool isSigned);
std::optional<spirv::CapabilityValue> capabilityFor(ScalarMapping mapping);
bool isFloatKind(ScalarKind kind);
uint32_t halfBitsOf(double value);
double halfValueOf(uint32_t bits);
bool isIntegerKind(ScalarKind kind);
uint64_t normalizeInteger(ScalarKind kind, uint64_t bits);
ScalarKind promotedKind(ScalarKind kind);
ScalarKind commonIntegerKind(ScalarKind left, ScalarKind right);
double roundedToKind(ScalarKind kind, double value);
double integerAsKind(ScalarKind kind, ScalarKind from, uint64_t integer);
bool isComparisonOperator(BinaryOperator op);
bool isStep(UnaryOperator op);
const char* binaryOperatorSpelling(BinaryOperator op);
uint16_t arithmeticOpcode(BinaryOperator op, bool isFloat, bool isSigned);
uint16_t comparisonOpcode(BinaryOperator op, bool isFloat, bool isSigned);
bool sameResourceType(const Type& have, const Type& want);

enum class MathShape {
	Componentwise,
	Saturate,
	Vector,
	VectorToScalar,
	Dot,
	Fwidth,
	Dfdx,
	Dfdy,
	Refract,
	Cross,
	RoundHalfAway,
	SignExact,
	FloatRemainder,
};

constexpr uint32_t kNoInstruction = 0;

struct MathBuiltin {
	const char* name;
	size_t arity;
	MathShape shape;
	uint32_t floatInstruction;
	uint32_t signedInstruction;
	uint32_t unsignedInstruction;
	bool inFastPrecise = true;
};

enum class PackEncoding { Unorm, Snorm, Srgb };

struct PackBuiltin {
	const char* name;
	bool pack;
	PackEncoding encoding;
	uint32_t count;
	uint32_t bits;
	ScalarKind floatKind;
};

const PackBuiltin* findPackBuiltin(const std::string& name);
bool isVoidCast(const Expression& expression);
bool isVoidValued(const Expression& expression);
const MathBuiltin* findMathBuiltin(const std::string& name);
std::optional<MathBuiltin> findScopedMathBuiltin(const std::string& callee);

struct StdlibOperand {
	ScalarKind scalar;
	uint32_t width;
	bool operator==(const StdlibOperand& other) const { return scalar == other.scalar && width == other.width; }
};

std::optional<StdlibOperand> stdlibOperand(const Parameter& parameter);
bool isStdlibSignature(const std::string& name, const std::vector<std::optional<StdlibOperand>>& operands);

class StageError: public CompileError {
public:
	using CompileError::CompileError;
};

constexpr uint32_t kFunctionControlNone = spirv::FunctionControl::None;
constexpr uint32_t kLinkageImport = 1;
constexpr uint32_t kLinkageExport = 0;
constexpr uint32_t kSelectionControlNone = spirv::SelectionControl::None;
constexpr uint32_t kLoopControlNone = spirv::LoopControl::None;
constexpr uint32_t kImageOperandsLod = 0x2;
constexpr uint32_t kImageOperandsOffset = 0x10;
constexpr uint32_t kGridSizeSpecializationID = 0x40000;
constexpr uint32_t kFunctionConstantSpecializationID = 0x10000;
constexpr uint32_t kFunctionConstantDefinedSpecializationID = 0x20000;

void collectIdentifiers(const Expression& expression, std::set<std::string>& names, bool includeFileScope = true);
void collectIdentifiers(const Statement& statement, std::set<std::string>& names, bool includeFileScope = true);
const Expression& writtenRoot(const Expression& target);
std::string unqualifiedName(const std::string& name);
using ReferenceSlots = std::set<std::pair<std::string, size_t>>;
void collectWritten(const Expression& expression, const ReferenceSlots& references, std::set<std::string>& names);
void collectWritten(const Statement& statement, const ReferenceSlots& references, std::set<std::string>& names);
bool callsBarrier(const Expression& expression);
bool callsBarrier(const Statement& statement);
using ScopeStack = std::vector<std::set<std::string>>;
void collectUnshadowed(const Expression* expression, const ScopeStack& scopes, std::set<std::string>& names);
void collectUnshadowed(const Statement* statement, ScopeStack& scopes, std::set<std::string>& names);

struct FoldedConstant {
	bool isComposite = false;
	Id type = InvalidId;
	std::vector<Id> parts;
	uint32_t matrixColumns = 0;
	uint32_t matrixRows = 0;
	std::vector<double> elements;

	ScalarKind scalar = ScalarKind::Void;
	double number = 0.0;
	uint64_t integer = 0;
	bool boolean = false;
};

struct Binding {
	Id id = InvalidId;
	bool isPointer = false;
	spirv::Id pointeeType = InvalidId;
	spirv::Id bufferPointeeType = InvalidId;
	uint32_t memberIndex = 0;
	spirv::StorageClassValue storageClass = spirv::StorageClass::Function;
	const StructDecl* structType = nullptr;
	Type pointeeMsl;
	uint32_t leadingLanes = 0;
	bool isBuffer = false;
	bool readOnly = false;
	bool isAddress = false;
	bool ownAddress = false;
	bool constantValue = false;
	bool unnormalizedSampler = false;
	std::optional<FoldedConstant> folded;
};

struct EmbeddedSampler {
	SamplerState state;
	Id variable = InvalidId;
};

class BindingScope {
public:
	explicit BindingScope(std::map<std::string, Binding>& bindings)
		: _bindings(bindings), _saved(bindings) {}
	~BindingScope() { _bindings = std::move(_saved); }
	BindingScope(const BindingScope&) = delete;
	BindingScope& operator=(const BindingScope&) = delete;

private:
	std::map<std::string, Binding>& _bindings;
	std::map<std::string, Binding> _saved;
};

class FileScopeView {
public:
	using Stack = std::vector<FileScopeView*>;
	explicit FileScopeView(Stack& views) : _views(views) {}
	FileScopeView(std::map<std::string, Binding>& bindings, Stack& views, const std::string& name,
		const Binding* array)
		: _bindings(&bindings), _views(views), _name(name), _hasArray(array != nullptr) {
		_local = bindings.extract(name);
		if (array) {
			bindings.emplace(name, *array);
		}
		_engaged = true;
		views.push_back(this);
	}
	FileScopeView(Stack& views, bool suspend) : _views(views) {
		if (!suspend) { return; }
		for (auto view = views.rbegin(); view != views.rend(); ++view) {
			if ((*view)->_hiding) {
				(*view)->release();
				_released.push_back(*view);
			}
		}
	}
	~FileScopeView() {
		if (_engaged) {
			release();
			_views.pop_back();
		}
		for (auto view = _released.rbegin(); view != _released.rend(); ++view) {
			(*view)->acquire();
		}
	}
	FileScopeView(const FileScopeView&) = delete;
	FileScopeView& operator=(const FileScopeView&) = delete;

private:
	void release() {
		if (!_hiding) { return; }
		if (_hasArray) { _file = _bindings->extract(_name); }
		if (!_local.empty()) { _bindings->insert(std::move(_local)); }
		_hiding = false;
	}
	void acquire() {
		_local = _bindings->extract(_name);
		if (_hasArray) { _bindings->insert(std::move(_file)); }
		_hiding = true;
	}

	std::map<std::string, Binding>* _bindings = nullptr;
	Stack& _views;
	std::vector<FileScopeView*> _released;
	std::map<std::string, Binding>::node_type _local;
	std::map<std::string, Binding>::node_type _file;
	std::string _name;
	bool _hasArray = false;
	bool _engaged = false;
	bool _hiding = true;

public:
	bool hides(const std::string& name) const { return _engaged && _hiding && _name == name; }
	bool hiding() const { return _engaged && _hiding; }
};

struct ConstantBinding {
	Id id = InvalidId;
	const StructDecl* structType = nullptr;
};

FoldedConstant convertConstant(FoldedConstant constant, ScalarKind to);
const char* cxxTypeName(ScalarKind kind);
std::string integerText(const FoldedConstant& constant);
void requireNotNarrowing(const FoldedConstant& constant, ScalarKind to);
bool fitsInKind(const FoldedConstant& constant, ScalarKind to);

struct StageVariable {
	Id variable = InvalidId;
	Id interfaceType = InvalidId;
	Id valueType = InvalidId;
	std::optional<uint32_t> attachment;
	ScalarKind sampled = ScalarKind::Float;
	std::string name;
};

void collectCalls(const Expression& expression, std::vector<const Expression*>& out);
void collectCalls(const VariableDeclaration& declaration, std::vector<const Expression*>& out);
void collectCalls(const Statement& statement, std::vector<const Expression*>& out);

constexpr std::string_view stdlibPrefix = "metal::";
std::set<std::string> overloadedNames(const TranslationUnit& unit);
std::string helperKey(const FunctionDecl& helper, const std::set<std::string>& overloaded);

enum ConversionRank : int { kExact, kPromotion, kConversion, kSplat, kNoConversion };

void validateHelpers(const TranslationUnit& unit, bool allowImports);

enum class SizeComponent : uint32_t { Width = 0, Height = 1, Depth = 2, ArraySize = 2 };

bool mentionsName(const Expression* expression, const std::string& name);
bool mentionsName(const Statement* statement, const std::string& name);

constexpr const char* kDereferenceOperand = "the operand of unary '*' has to be a pointer "
	"parameter; a dereference of an element, a call or other expression is not supported";
std::string notAPointer(const std::string& name);
std::optional<int64_t> literalIndex(const Expression& expression);
size_t fieldIndexOf(const StructDecl& decl, const std::string& member, std::string& outDeclName);

struct AccessStep {
	bool isIndex = false;
	bool isDereference = false;
	bool isArrow = false;
	const Expression* index = nullptr;
	std::string memberName;
};

const Expression* accessChain(const Expression& expression, std::string& outName,
	std::vector<AccessStep>& outSteps);
std::vector<uint32_t> swizzleLanes(const std::string& name);
void requireLanesWithin(const std::string& name, const std::vector<uint32_t>& lanes, uint32_t width);

class ControlDepthScope {
public:
	explicit ControlDepthScope(uint32_t& depth): _depth(depth) {
		constexpr uint32_t kMaxControlDepth = 1000;
		if (_depth >= kMaxControlDepth) {
			throw CompileError("control flow is nested more than "
				+ std::to_string(kMaxControlDepth) + " deep");
		}
		++_depth;
	}
	~ControlDepthScope() { --_depth; }
	ControlDepthScope(const ControlDepthScope&) = delete;
	ControlDepthScope& operator=(const ControlDepthScope&) = delete;

private:
	uint32_t& _depth;
};

bool containsBranchingOperator(const Expression& expression);
bool isAtomicFunction(const std::string& name);
std::string builtinCalleeName(const std::string& name);
void collectMemFlags(const Expression& flags, const std::string& function,
	uint32_t& semantics, bool& crossesWorkgroup);
bool isAsTypeCall(const Expression& call);
std::string templateMarker(const TemplateParameter& parameter);
bool isTemplateMarker(const Type& type);
std::string templateArgumentName(const TemplateArgument& argument);
bool fitsTemplateConstant(ScalarKind kind, int64_t value);
bool sameValueType(const Type& a, const Type& b);
bool dependsOnParameter(const Expression& expression);
void rejectPlaceThroughConditional(const Expression& target, const char* what);
bool sameArrayType(const Type& expected, const Type& got);
void requireArrayElementMatch(const Type& expected, const Expression& expression);
std::string lodNumber(float value);
bool isArrayKind(ResourceKind kind);
bool isCubeKind(ResourceKind kind);
bool isOneDimensional(ResourceKind kind);
uint32_t textureCoordinateWidth(ResourceKind kind);
void requireTexture1DLevelZero(const Expression& lod, const std::string& callName);
size_t layerArguments(ResourceKind kind);
uint32_t storageCoordinateWidth(ResourceKind kind);
uint32_t textureSizeWidth(ResourceKind kind);
bool literalInteger(const Expression& expression, int32_t& value);
Type boundArray(const Type& parameter, uint32_t length);
std::vector<std::optional<uint32_t>> stageLocations(const StructDecl& decl);

} // namespace mslc
