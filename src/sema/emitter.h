#pragma once

#include "sema/internal.h"

namespace mslc {

class Emitter {
	spirv::Builder& _builder;
	const TranslationUnit& _unit;
	// The module's entry points, in declaration order. Several of them is
	// ordinary: Metal compiles a whole file, so one source carrying a vertex
	// and a fragment function is one module with two entry points.
	const std::vector<const FunctionDecl*>& _entryPoints;
	// The one being emitted. Everything stage-dependent reads it, so a second
	// entry point in another stage gets its own set and its own execution
	// modes without any of them being passed down.
	const FunctionDecl* _entryPoint = nullptr;
	// Stands in for the entry point while a library's helpers are emitted: no
	// stage, so a helper that needs one (fwidth, a barrier) is refused.
	FunctionDecl _libraryScope;
	const ModuleOptions& _options;
	TypeTable& _types;

	std::map<std::string, Binding> _bindings;
	// File-scope constants, keyed by name. Separate from _bindings because a
	// constant belongs to the module rather than to the entry point, and an
	// entry point's own names are looked up first.
	std::map<std::string, ConstantBinding> _constants;
	// The file-scope function constants, keyed by name. Their specialization constants are declared when
	// the source first reads them, so a constant nothing reads asks for no capability its type needs.
	struct FunctionConstant {
		const VariableDeclaration* declaration = nullptr;
		Id value = InvalidId;
		Id defined = InvalidId;
	};
	std::map<std::string, FunctionConstant> _functionConstants;
	// "constant bool b = is_function_constant_defined(c);": b, and the c it stands for.
	std::map<std::string, std::string> _functionConstantDefinedAliases;
	// A file-scope constant whose initialiser reads a function constant, as "constant bool b = n > 0;" with n
	// one: a specialization constant computed from those (OpSpecConstantOp), declared where it is first read.
	// reads is every function constant under it, through other such constants too, for the reflection.
	struct DerivedFunctionConstant {
		const VariableDeclaration* declaration = nullptr;
		Id value = InvalidId;
		bool emitting = false;
		std::set<std::string> reads;
	};
	std::map<std::string, DerivedFunctionConstant> _derivedFunctionConstants;
	// The calls emitted so far whose function returns a reference, which is lowered as a value.
	std::set<const Expression*> _referenceCalls;
	// The function constants each function reads in the code emitted for it, and the functions each calls:
	// an entry point's reflection lists the constants of every function it reaches. A helper is emitted once
	// for the entry points that share it, so what it reads is kept with it rather than with an entry point.
	std::map<const FunctionDecl*, std::set<std::string>> _functionConstantReads;
	std::map<const FunctionDecl*, std::set<const FunctionDecl*>> _functionCallees;
	// The functions whose emitted code calls get_num_samples(), kept the same way.
	std::set<const FunctionDecl*> _numSamplesReaders;
	// The file-scope samplers, by name, and the ones the entry point being
	// emitted names, which are looked up after its own parameters and locals.
	std::map<std::string, const VariableDeclaration*> _fileScopeSamplers;
	std::map<std::string, Binding> _fileScopeSamplerBindings;
	// The functions that name a file-scope sampler, or call one that does. Each entry point
	// that reaches one gets its own copy, since the sampler is that entry point's variable.
	std::set<const FunctionDecl*> _samplerDependent;
	// What each constant folded to, so a later constant referring to this one
	// needs the value rather than a reference to a constant.
	std::map<std::string, FoldedConstant> _folded;
	std::map<Id, FoldedConstant> _localFolded;
	// Set when a fold meets an operation a constant expression may not contain:
	// a signed overflow, a division by zero, a shift by a bad count, a NaN. The
	// value is still computed, since only a constexpr needs it to be an error.
	bool _foldDomainError = false;
	std::map<spirv::Id, const StructDecl*> _structByValue;
	// The pointee of every buffer parameter, in declaration order, which is
	// what the binding-0 block is built from once the loop is done.
	std::vector<Id> _bufferMembers;
	// The bool or bool vector type of a member that holds one by reference.
	std::map<uint32_t, Id> _bufferBoolTypes;
	// The matrix type of a member that holds one by reference, wrapped in a struct.
	std::map<uint32_t, Id> _bufferMatrixTypes;
	// The binding-0 block, and the loaded buffer pointer for each member.
	TypeTable::AddressBlock _addressBlock;
	std::map<uint32_t, Id> _bufferBases;
	// Addresses of a packed vector in a buffer. The pointee is the same SPIR-V
	// vector as a float3's, so the address is what remembers that it is only as
	// aligned as a component.
	std::set<Id> _packedAddresses;
	// Addresses of a bool or a bool vector in a buffer, each with the bool type
	// the access reads and writes. The address points at bytes, so what the
	// address itself says would be taken for a uchar.
	std::map<Id, Id> _boolAddresses;

	Id _uintType = InvalidId;
	Id _intType = InvalidId;
	Id _boolType = InvalidId;
	Id _voidType = InvalidId;
	Id _functionId = InvalidId;
	Id _entryPointId = InvalidId;
	std::vector<Id> _entryPointIds;
	bool _floatControlsDeclared = false;
	spirv::Id _glslSet = InvalidId;

	// Set once the current block has its terminator, so nothing more may be
	// emitted into it until the next label.
	bool _terminated = false;

	// Whether a statement followed a terminator, and whether to lower such statements
	// (in a scratch module, for Apple's diagnostics) instead of skipping them.
	bool _sawDeadCode = false;
	bool _lowerDeadCode = false;

	// The label of the block being emitted into, which an OpPhi names as the
	// predecessor a value arrives from.
	Id _currentBlock = InvalidId;

	// How many selection and loop constructs enclose the point being emitted,
	// counting the right operand of a scalar && or ||: spirv-val rejects a module
	// nested 1023 deep, whatever the source nesting that produced it.
	uint32_t _controlDepth = 0;

	// Where break and continue go from the point being emitted, innermost
	// last. A target with no continue label is one a switch could push: break
	// reaches it, continue looks past it to the loop around.
	struct JumpTarget {
		Id breakLabel;
		Id continueLabel;
	};
	std::vector<JumpTarget> _jumpTargets;

	std::string _reflection;
	// The next descriptor binding a texture or sampler takes, and the
	// constexpr samplers declared so far in the entry point.
	uint32_t _nextBinding = 0;
	std::vector<EmbeddedSampler> _embeddedSamplers;
	// Set once the module needs ImageQuery, which is declared once.
	bool _imageQueryDeclared = false;
	bool _gatherExtendedDeclared = false;
	bool _minLodDeclared = false;
	std::map<int32_t, Id> _intConstants;
	bool _stencilExportDeclared = false;
	bool _sampleRateShadingDeclared = false;
	// Ids the entry point lists as its interface, in declaration order.
	std::vector<Id> _interface;
	// The Workgroup variable of each threadgroup local in the kernel body, declared
	// before the entry point because SPIR-V 1.5 lists every module-scope variable in it.
	std::map<const VariableDeclaration*, Id> _threadgroupLocals;
	// The next SpecId for a threadgroup argument's element count; 0, 1 and 2 are the workgroup size.
	uint32_t _nextSpecId = 3;

	// What a return statement writes: one variable for a returned scalar or
	// vector, or one per field of a returned struct. Empty for a void function.
	std::vector<StageVariable> _outputs;
	// The [[stage_in]] parameter and one Input per field.
	const Parameter* _stageIn = nullptr;
	// The kernel's [[threads_per_threadgroup]] parameter, and the spec constants
	// that hold the workgroup size it reads.
	const Parameter* _threadsPerThreadgroup = nullptr;
	Id _localSize[3] = { InvalidId, InvalidId, InvalidId };
	// The grid size of a kernel that skips the threads outside it (ModuleOptions::maskGridSize): three
	// spec constants shared by the module, and the Input variable of the thread position in the grid
	// the guard reads, which is the kernel's own [[thread_position_in_grid]] when it has one.
	Id _gridSize[3] = { InvalidId, InvalidId, InvalidId };
	Id _globalInvocationVariable = InvalidId;
	bool _masksGridSize = false;
	// The sample count get_num_samples() reads, one spec constant for the module.
	Id _numSamples = InvalidId;
	std::vector<StageVariable> _stageInputs;
	// Framebuffer fetch: a [[color(n)]] parameter and the value it is read into.
	struct AttachmentParameter {
		const Parameter* parameter;
		StageVariable variable;
	};
	std::vector<AttachmentParameter> _attachmentParameters;
	std::set<uint32_t> _attachmentIndices;
	// At most one BuiltIn FragCoord Input variable per fragment entry point,
	// whether it comes from a stage-struct [[position]] field or a direct
	// [[position]]/[[frag_coord]] parameter.
	bool _hasFragCoordInput = false;

	// A helper's body is emitted once per stage that reaches it, or once per entry point when it
	// names a file-scope sampler (helperScope).
	struct HelperFunction {
		const FunctionDecl* definition = nullptr;
		std::map<std::pair<Stage, const FunctionDecl*>, Id> ids;
		Id returnType = InvalidId;
		std::vector<Id> parameterTypes;
		Id type = InvalidId;
		// For a helper with a thread pointer parameter, the length of the caller's
		// array bound to each parameter (zero for any other): the helper is
		// emitted once per distinct list, so its pointer is to the whole array.
		std::vector<uint32_t> arrayLengths;
		// For a threadgroup pointer parameter, the caller's threadgroup variable (InvalidId for any other).
		// A Workgroup variable is the module's own, so the helper is emitted once per list of them and names
		// each directly rather than taking a pointer: drivers mishandle a Workgroup pointer parameter (the
		// NVIDIA one loses stores made through one), and glslang never emits one.
		std::vector<Id> threadgroupVariables;
		// Where the first declaration of this signature sits (FunctionDecl::order):
		// an overload is a candidate only from there down.
		size_t firstOrder = 0;
	};
	// Keyed by helperKey: the name, or for an overloaded name each signature.
	std::map<std::string, HelperFunction> _helpers;
	std::set<std::string> _overloadedNames;
	std::map<std::string, std::vector<HelperFunction*>> _overloads;
	// Which function calls which, so a cycle through overloads, which the by-name check
	// of validateHelpers cannot see, is refused when the call that closes it is emitted.
	std::map<const FunctionDecl*, std::set<const FunctionDecl*>> _callEdges;
	std::map<std::pair<const FunctionDecl*, std::vector<uint32_t>>, HelperFunction> _pointerSpecialisations;
	// Helpers called and not yet emitted. A body cannot be emitted while the
	// function that calls it is half written, so they wait for its end.
	std::vector<HelperFunction*> _helperQueue;
	// A member call's receiver, evaluated early to learn its struct.
	Id _pendingObject = InvalidId;
	std::map<std::vector<Id>, Id> _functionTypes;
	// The helper whose body is being emitted, or null inside an entry point.
	const FunctionDecl* _helper = nullptr;
	// The return type of each function declared "auto f(...)", once its first return has been emitted.
	std::map<const FunctionDecl*, Type> _deducedReturns;
	// The functions being emitted to deduce their return type, innermost last.
	std::set<const FunctionDecl*> _deducing;
	// Functions emitted inside another's body to deduce their return type, put after that body ends; and how
	// many such emissions are under way.
	std::vector<spirv::Instruction> _deferredFunctions;
	unsigned _nestedHelpers = 0;

public:
	Emitter(spirv::Builder& builder, const TranslationUnit& unit,
		const std::vector<const FunctionDecl*>& entryPoints,
		const ModuleOptions& options, TypeTable& types):
		_builder(builder), _unit(unit), _entryPoints(entryPoints),
		_options(options), _types(types) {
		for (const FunctionTemplate& declaration: unit.templates) {
			_templatesByName[declaration.name].push_back(&declaration);
			if (!declaration.memberOf.empty()) {
				_memberTemplateNames.insert(declaration.name.substr(declaration.name.rfind("::") + 2));
			}
		}
	}

	std::string run();
	bool sawDeadCode() const { return _sawDeadCode; }
	void lowerDeadCode() { _lowerDeadCode = true; }

private:
	void emitEntryPoint(spirv::Id functionType);
	Id constantU32(uint32_t value);
// indium splits descriptor sets by stage: set 0 from the vertex function,
// set 1 from the fragment function. A kernel has one set.
uint32_t descriptorSet() const {
	return _entryPoint->stage == Stage::Fragment ? 1u : 0u;
}
	Id bufferBase(const Binding& binding);
	void preloadBufferBases();
	// The laid-out element type of a device or constant pointer's pointee, and
	// the { T runtime_array[] } struct the pointer is to: what an entry point's
	// own buffer parameter binds and what such a helper parameter takes.
	std::pair<Id, Id> bufferParamTypes(const Type& type);
	void loadThreadsPerThreadgroup();
	void declareGridMaskInput();
	void emitGridMask();
	// The address of a packed vector's storage, viewed as a pointer to the
	// vector itself, which is what a load or store wants.
	Id packedVectorAddress(Id storageAddress, Id vectorType);
	Id loadFromBuffer(Id pointer, Id pointeeType);
	void storeIntoBuffer(Id pointer, Id value);
	// The address of a bool in a buffer, which holds bytes, recorded as one so
	// that a load and a store through it convert.
	Id boolAddress(Id storageAddress, Id boolType);
	// What an address points at, as the source sees it: the bool type for a
	// bool's bytes, the pointee otherwise.
	Id valueTypeAt(Id address) const;
	Id boolFromStorage(Id bytes, Id boolType);
	Id boolToStorage(Id value);
	// The Aligned memory operand a buffer access has to carry, at the
	// alignment the buffer's own layout guarantees.
	std::vector<uint32_t> alignedOperands(Id pointer, Id pointeeType) const;
	Id declaredTypeOf(const Type& type);
	void declareGlobals();
	Id declareGlobalConstant(const VariableDeclaration& declaration);
	Id emitConstant(const FoldedConstant& folded);
	std::vector<uint32_t> constantWords(const FoldedConstant& folded);
	FoldedConstant foldScalarConstruct(const Expression& expression);
	FoldedConstant convertScalarForConstruct(FoldedConstant folded, ScalarKind to);
	std::vector<FoldedConstant> foldVectorConstructor(const Expression& expression);
	FoldedConstant foldConditional(const Expression& expression);
	bool constantLeaves(const Expression& expression, const std::string& excluded);
	bool namesConstexprAggregate(const Expression& expression);
	bool provenByFolding(const Expression& expression, const std::string& excluded);
	FoldedConstant foldComparison(BinaryOperator op, FoldedConstant left, FoldedConstant right);
	FoldedConstant foldInitializer(const Type& type, const Expression& initializer, bool braced = false);
	void foldStructInitializer(FoldedConstant& folded, const StructDecl& decl,
		const Expression& initializer);
	FoldedConstant foldMatrix(const Expression& expression, const Type* expected);
	FoldedConstant foldMatrixList(const Type& type, const Expression& initializer);
	FoldedConstant foldMatrixConstruct(const Expression& expression);
	FoldedConstant foldMatrixProduct(const Expression& expression);
	FoldedConstant matrixConstant(ScalarKind kind, uint32_t columns, uint32_t rows,
		std::vector<double> elements);
	bool isMatrixExpression(const Expression& expression);
	std::vector<double> foldColumnConstructor(const Expression& expression, ScalarKind kind, uint32_t rows);
	double foldMatrixScalar(const Expression& expression, ScalarKind kind, bool braced);
	FoldedConstant foldExpression(const Expression& expression);
	FoldedConstant foldBinary(const Expression& expression);
	FoldedConstant foldUnary(const Expression& expression);
	void declareParameters();
	void declareThreadgroupArgument(const Parameter& parameter, size_t index, uint32_t metalIndex);
	void declareResources(const std::vector<std::pair<size_t, const Parameter*>>& resources);
	Id declareDescriptorVariable(Id pointee, uint32_t binding);
	void copyWrittenBuiltins();
	// Whether it found any; reserve false only looks.
	bool reserveLocalSamplers(const Statement& statement, const std::set<std::string>& used, bool reserve = true);
	void reserveThreadgroupLocals(const Statement& statement);
	void reserveThreadgroupLocal(const VariableDeclaration& declaration);
	void reserveSampler(const VariableDeclaration& declaration);
	// Look up an identifier's value/storage binding, skipping locals when marked file-scope ("::name").
	const Binding* findBinding(const Expression& id) const {
		if (id.kind != ExpressionKind::Identifier) return nullptr;
		const auto& table = id.fileScope ? _fileScopeBound : _bindings;
		const auto it = table.find(id.name);
		return it == table.end() ? nullptr : &it->second;
	}
	// The entry point's own name first (or file-scope arrays for "::name"), then a file-scope sampler it names.
	// A helper the pre-emission scan did not mark would share one body between entry points, so
	// a file-scope sampler it names is refused: that is argument-dependent lookup, a template
	// instance or any other path the scan does not follow.
	const Binding* findResourceBinding(const std::string& name, bool fileScope = false) const {
		const auto& table = fileScope ? _fileScopeBound : _bindings;
		const auto own = table.find(name);
		if (own != table.end()) return &own->second;
		const auto fileScopeSampler = _fileScopeSamplerBindings.find(name);
		if (fileScopeSampler == _fileScopeSamplerBindings.end()) return nullptr;
		if (_helper && _helper->body && !_samplerDependent.count(_helper)) {
			throw CompileError("\"" + _helper->name + "\" names the file-scope sampler \"" + name
				+ "\" but is reached by a path mslc does not follow (argument-dependent lookup or a "
				"template instance); pass the sampler as a parameter or call it by a qualified name");
		}
		return &fileScopeSampler->second;
	}
	Binding embeddedSamplerBinding(const VariableDeclaration& declaration) const;
	void reserveFileScopeSamplers();
	void declareFunctionConstant(const VariableDeclaration& declaration);
	void declareFunctionConstantAlias(const VariableDeclaration& declaration);
	bool isFunctionConstantDefinedAlias(const VariableDeclaration& declaration) const;
	const std::string* functionConstantNamed(const std::string& name) const;
	Id functionConstantValue(const std::string& name, bool defined);
	void declareDerivedFunctionConstant(const VariableDeclaration& declaration);
	Id derivedFunctionConstantValue(const std::string& name);
	Id gateValue(const std::string& name);
	Id emitSpecConstantExpression(const Expression& expression, const std::string& constant);
	Id specConstantConvert(Id value, Id type);
	Id emitIsFunctionConstantDefined(const Expression& call);
	Id emitNumSamples(const Expression& call);
	Id emitAsType(const Expression& call);
	std::string functionConstantsJson();
	std::set<const FunctionDecl*> reachedFunctions();
	std::vector<const FunctionDecl*> functionsCalledBy(const Expression& call);
	void declareLocalSampler(const VariableDeclaration& declaration);
	void bindResourceAlias(const VariableDeclaration& declaration, const Type* declared);
	void addReflectionEntry(const std::string& entry);
	std::string descriptorJson(uint32_t binding) const;
	Binding textureReceiver(const Expression& call);
	// The descriptor an expression names, worked out while compiling, and whether the expression is its
	// address rather than the texture or sampler itself.
	struct ResolvedResource {
		Binding binding;
		bool pointer = false;
	};
	std::optional<ResolvedResource> resourceOf(const Expression& expression, unsigned depth = 0);
	std::optional<ResolvedResource> resourceCall(const Expression& call, unsigned depth);
	Binding resourceNamed(const Expression& expression, bool pointer, const std::string& what);
	void emitTextureWrite(const Expression& call, const Binding& texture);
	void emitTextureFence(const Expression& call, const Binding& texture);
	Id emitTextureCall(const Expression& call);
	Id emitTextureSample(const Expression& call, const Binding& texture, bool compare = false);
	Id compareValue(const Expression& argument, const std::string& callName);
	Id emitTextureRead(const Expression& call, const Binding& texture);
	Id emitMultisampledCall(const Expression& call, const Binding& texture);
	Id emitTextureGather(const Expression& call, const Binding& texture, bool compare = false);
	Binding samplerArgument(const Expression& argument, ResourceKind kind, const std::string& callName);
	Id samplingCoordinate(const Expression& coordinate, ResourceKind kind, const Expression* layer, const std::string& callName);
	Id emitTextureSize(const Expression& call, const Binding& texture, SizeComponent component);
	Id literalOffset(const Expression& expression, uint32_t width);
	Id layerIndex(const Expression& layer, const std::string& callName, const char* what = "array index");
	Id appendLayers(Id coordinate, ResourceKind kind, const std::vector<ExpressionPtr>& arguments, size_t first,
		const std::string& callName);
	Id appendArrayLayer(Id coordinate, ScalarKind component, const Expression& layer,
		const std::string& callName);
	Id texturePixels(const Binding& texture, Id sampled);
	Id textureTexel(const Binding& texture, Id sampled);
	Id emitLod(const Expression& expression, const std::string& callName);
	const StructDecl* structValue(const Type& type) const;
	StageVariable declareStageVariable(const Type& type, spirv::StorageClassValue storageClass,
		const std::string& what);
	void decorateInterpolation(Id variable, Interpolation interpolation);
	std::vector<StageVariable> declareFragmentOutputs(const StructDecl& decl);
	std::vector<StageVariable> declareStageStruct(const StructDecl& decl,
		spirv::StorageClassValue storageClass);
	std::vector<StageVariable> declareVertexAttributes(const StructDecl& decl);
	void declareStageOutputs();
	void loadStageInputs();
	StageVariable declareFramebufferFetch(const Type& type, uint32_t index, const std::string& name,
		const std::string& what);
	void declareAttachments();
	Id readAttachment(const StageVariable& attachment);
	void emitReturn(const Statement& statement);
	void checkStageInterfacesAgree() const;
	void emitFunctionBody(const Statement& statement);
	void emitStatement(const Statement& statement);
	void emitSwitch(const Statement& statement);
	void emitExpressionStatement(const Expression& expression);
	void emitAssignment(const Expression& left, const Expression& right,
		std::optional<BinaryOperator> compound);
	Id emitPlaceAddress(const Expression& left);
	const Expression& storeRoot(const Expression& target);
	bool readOnlyRoot(const Expression& root);
	void requireStorable(const Expression& target);
	void emitSwizzleStore(const Expression& target, const Expression& valueExpression,
		std::optional<BinaryOperator> compound);
	void emitVariableDeclaration(const VariableDeclaration& declaration, bool storageOnly = false);
	void emitValueDeclaration(const VariableDeclaration& declaration, const Type& type, Id deduced,
		bool storageOnly);
	void emitAutoDeclaration(const VariableDeclaration& declaration);
	Type deduceAutoType(const Type& written, Id valueType, const std::string& name);
	// storage is where the variable itself lives: Function for an ordinary
	// local, Workgroup for a threadgroup one. It is what the index path walks
	// the binding with, so it has to be the real one.
	void bindLocal(const std::string& name, Id variable, Id type, const Type& msl,
		spirv::StorageClassValue storage = spirv::StorageClass::Function);

	// Whether a binding names a variable this compiler declares on its own,
	// rather than one reached through a descriptor. An array lives in
	// whichever of these its address space selects, and every path that walks
	// one has to agree on the list: an array path that omits Workgroup
	// reports a misleading diagnostic or emits an access chain typed at the
	// wrong storage class, and neither is caught until something downstream
	// rejects it. So this is the single place the list lives.
	bool isLocalStorage(spirv::StorageClassValue storage) const;

	// A threadgroup variable is shared by every invocation of the workgroup,
	// so it exists in a kernel and nowhere else, and it takes no initialiser.
	// A scalar and an array share this, so the two cannot drift apart.
	void checkThreadgroupLocal(const VariableDeclaration& declaration);
	void beginBlock(Id label);
	void terminate(uint16_t opcode, std::vector<uint32_t> operands);
	void branchUnlessTerminated(Id label);

	// Every expression returns a value id whose type the builder knows.
	// For an lvalue such as "buffer[index]" the result is a pointer, and
	// the caller loads from it.
	Id emitExpression(const Expression& expression);
	Id emitBinary(const Expression& expression);
	Id emitBinaryOperation(BinaryOperator op, Id left, Id right);
	Id emitShortCircuit(BinaryOperator op, Id left, const Expression& rightExpression);
	Id emitConditional(const Expression& expression);
	bool isSafeToEvaluateUnchosen(const Expression& expression) const;
	Id conditionalType(Id trueType, Id falseType);
	Id toConditionalType(Id value, Id type);
	Id emitArithmetic(BinaryOperator op, Id left, Id right);
	Id emitVectorComparison(BinaryOperator op, Id left, Id right);
	Id emitCompoundOperation(BinaryOperator op, Id current, Id value);
	struct ArithmeticConversion {
		Id type;
		bool isSigned;
	};
	ArithmeticConversion usualArithmeticConversion(Id leftType, Id rightType);
	Id emitMatrixProduct(BinaryOperator op, Id left, Id right);
	Id emitUnary(const Expression& expression);
	Id emitUnaryOperation(UnaryOperator op, Id operand, Id type);
	void emitReferenceDeclaration(const VariableDeclaration& declaration);
	void emitLocalArray(const VariableDeclaration& declaration, const Type& type, bool storageOnly);
	void declareThreadgroupLocal(const VariableDeclaration& declaration, const Type& type, Id typeId);
	Id arrayTypeOf(const Type& type);
	struct ArrayLeaves {
		// The value of one element given its list entry, or its zero fill for none.
		std::function<Id(const Type& element, const Expression* entry)> element;
		std::function<Id(Id type, const std::vector<uint32_t>& parts)> compose;
	};
	Id emitArrayInit(const Type& type, const std::vector<InitializerElement>& entries, size_t& position,
		bool braced, const ArrayLeaves& leaves);
	Id emitArrayListValue(const Type& type, const Expression& list);
	Id emitLocalArrayIndex(const Expression& expression, bool asAddress);
	const Binding* arrayRootBinding(const Expression& expression, size_t& depth) const;
	void declareFileScopeArray(const VariableDeclaration& declaration, size_t position);
	void bindFileScopeArrays(size_t declaredBefore);
	std::unique_ptr<FileScopeView> viewFileScope(const Expression& expression);
	FileScopeView::Stack _fileScopeViews;
	struct FileScopeArray {
		std::string name;
		size_t position = 0;
		Id variable = InvalidId;
		Id type = InvalidId;
		Type msl;
	};
	std::vector<FileScopeArray> _fileScopeArrays;
	// The file-scope arrays the function being emitted sees, as bound before any local.
	std::map<std::string, Binding> _fileScopeBound;
	void validateConstexprInitializer(const VariableDeclaration& declaration);
	void validateConstexprInitializer(const VariableDeclaration& declaration, const Type& type);
	std::optional<std::string> enumOf(const Expression& expression);
	bool isComponentValue(const Expression& expression);
	void checkBinaryComponentOperands(BinaryOperator op, const Expression& left, const Expression& right);
	void checkConditionalOperands(const Expression& condition, const Expression& whenTrue,
		const Expression& whenFalse);
	Id emitNonComponent(const Expression& expression, const std::string& what);
	void requireEnumValue(const std::string& target, const Expression& value, const std::string& what);
	bool readsRuntimeValue(const Expression& expression) const;
	void requireSameReferent(const VariableDeclaration& declaration, const Type& type, Id referent);
	std::optional<Type> arrayExpressionType(const Expression& expression);
	const StructDecl* indexOperatorOwner(const Expression& expression);
	HelperFunction* indexOperatorHelper(const StructDecl& owner);
	bool validMemberReceiver(const FunctionDecl& definition, const Expression& receiver);
	bool isComputedValue(const Expression& expression);
	bool isTemporaryPart(const Expression& target);
	bool isStructValue(const Expression& expression);
	Id emitIndex(const Expression& expression, bool asAddress, bool allowVoid = false);
	bool isElementAccess(const Expression& expression) const;
	Id emitElementAccess(const Expression& expression, bool asAddress);
	struct ElementIndex {
		std::optional<uint32_t> constant;
		Id id = InvalidId;
	};
	ElementIndex elementIndex(const Expression& expression, uint32_t width);
	Id emitMatrixValueColumn(Id matrixValue, const TypeTable::MatrixInfo& matrix, const Expression& index);
	Id wordIndex(Id index, const char* what);
	Id stepIndex(const Expression& expression, const char* what);
	Id emitMember(const Expression& expression);
	const Type* pointerMemberType(const Expression& expression);
	std::optional<Binding> pointerMemberBinding(const Expression& expression);
	size_t arrayMemberRank(const Expression& expression, size_t& outIndices);
	std::optional<Id> emitArrayMemberElement(const Expression& expression, bool asAddress);
	Id emitMemberAddress(const Expression& expression, Id& outFieldType);

	// The file-scope constant an expression names, or null when it names
	// something else. A constant is a value and not a place, which is what
	// tells a member read on one apart from a member read on a local.
	const ConstantBinding* constantFor(const Expression* expression);
	Id emitCall(const Expression& expression);
	HelperFunction* findHelper(const Expression& call, const Expression*& receiver, std::vector<Id>& prepared);
	Id emitHelperCall(const Expression& call, HelperFunction& helper, const Expression* receiver,
		const std::vector<Id>& prepared);
	std::vector<HelperFunction*> helpersNamed(const std::string& name);
	std::vector<std::string> namesOfCall(const Expression& call, std::set<std::string>* adlOnly = nullptr);
	const StructDecl* adlStructOf(const Expression& argument);
	bool inConstantSpace(const Expression& place);
	std::optional<Type> placeType(const Expression& place);
	void checkReferenceReturned(const Expression& returned, std::vector<const Expression*>& calls);
	std::optional<Type> sizeofPlaceType(const Expression& place);
	std::optional<Type> sizeofExpressionType(const Expression& expression);
	bool namesPointerParameter(const std::string& name) const;
	uint64_t sizeofOperand(const Expression& operand);
	bool isVectorPlace(const Expression& place);
	bool isVectorComponent(const Expression& place);
	bool bindsTemporary(const Expression& place);
	std::optional<Type> computedPartType(const Expression& part);
	Id constantReferenceArgument(const Expression& argument, const Parameter& parameter,
		const std::string& function, size_t position, Id prepared);

	// A template's signature, read with the arguments deduced from a call's, is an instance: a helper
	// that competes by the usual ranks, and has its body read only if it is chosen.
	static constexpr size_t kMaxInstantiationDepth = 64;
	struct TemplateInstance {
		const FunctionTemplate* declaration = nullptr;
		std::vector<TemplateArgument> arguments;
		FunctionDecl decl;
		HelperFunction helper;
		size_t depth = 0;
		bool committed = false;
		// instantiationOrder() for the calls in the instance: set when it is first committed.
		size_t rootOrder = 0;
	};
	std::map<std::string, std::vector<const FunctionTemplate*>> _templatesByName;
	std::map<std::string, std::unique_ptr<TemplateInstance>> _instances;
	std::map<const FunctionDecl*, TemplateInstance*> _instanceOf;
	std::map<std::string, std::unique_ptr<FunctionDecl>> _patterns;
	// The explicit specialization that is the body of an instance, by the instance's key.
	std::map<std::string, const ExplicitSpecialization*> _specializationOf;
	std::string instanceKey(const FunctionTemplate& declaration, const std::vector<TemplateArgument>& arguments) const;
	void resolveSpecializations();
	const ExplicitSpecialization* specializationFor(const TemplateInstance& instance) const;
	std::vector<const FunctionTemplate*> templatesNamed(const std::string& name) const;
	std::vector<const FunctionTemplate*> visibleTemplates(const std::vector<std::string>& names,
		const std::set<std::string>& adl) const;
	std::vector<const FunctionTemplate*> templatesCalled(const Expression& call) const;
	const FunctionDecl* templatePattern(const FunctionTemplate& declaration, const std::vector<TemplateArgument>& given,
		std::vector<TemplateArgument>& arguments);
	struct OverloadArgument;
	bool deduceTemplateArguments(const FunctionTemplate& declaration, const FunctionDecl& pattern,
		size_t given, const std::vector<OverloadArgument>& arguments, std::vector<TemplateArgument>& deduced);
	TemplateInstance& templateInstance(const FunctionTemplate& declaration, std::vector<TemplateArgument> arguments);
	void commitInstance(TemplateInstance& instance);
	bool isInstance(const FunctionDecl* function) const { return function && _instanceOf.count(function) != 0; }
	bool callsDependentTemplate(const Expression& call);
	bool deducedResultIsFundamental(const Expression& call);
	bool bringsNoNamespace(const Expression& argument);
	bool dependsOnTemplate(const Expression& call);
	std::string describeTemplate(const FunctionTemplate& declaration, const FunctionDecl& pattern) const;
	HelperFunction* definedHelper(HelperFunction* entry);
	// What a call can denote before its arguments are known: every function of every name the
	// parser found for it (not the ones argument-dependent lookup adds).
	std::vector<HelperFunction*> helpersCalled(const Expression& call);
	// A member call's receiver is the argument of the first parameter, "this", and is already evaluated.
	HelperFunction* resolveOverload(const Expression& call, const std::vector<HelperFunction*>& set,
		const std::vector<const FunctionTemplate*>& templates, const std::set<std::string>& adl,
		std::vector<Id>& prepared, const Expression* receiver = nullptr);
	// The member function templates of a struct that a call through "." names, or none.
	std::vector<const FunctionTemplate*> memberTemplatesOf(const std::string& owner, const std::string& member) const;
	std::set<std::string> _memberTemplateNames;
	const Type* returningType(const Expression& call, std::string (*aspect)(const Type&));
	size_t callerOrder() const { return (_helper ? _helper : _entryPoint)->order; }
	// Where a call is instantiated, as clang places it: for a call in a template's instance, after the function
	// outside any template whose call led to the instance; for any other call, the caller.
	size_t instantiationOrder() const {
		const auto instance = _helper ? _instanceOf.find(_helper) : _instanceOf.end();
		return instance == _instanceOf.end() ? callerOrder() : instance->second->rootOrder;
	}
	void noteCall(const FunctionDecl& callee);
	bool isPlainVariable(const Expression& expression) const;
	bool isWholeVariable(const Expression& expression) const;
	struct PointerFit {
		Type element;
		Type wanted;
		std::vector<uint32_t> shape;
		std::vector<uint32_t> wantedShape;
		bool matches = false;
	};
	PointerFit pointerFit(const Binding& array, const Type& parameter);
	// What a call's argument is, as far as choosing an overload cares: a value, a variable
	// (a place a reference can bind), a local array (what a pointer parameter takes), or a
	// texture or sampler. Only the first two are evaluated.
	struct OverloadArgument {
		enum class Kind { Value, Variable, Array, Resource } kind = Kind::Value;
		const Expression* expression = nullptr;
		const Binding* binding = nullptr;
		Id value = InvalidId;
		std::optional<std::string> tag;
		// A non-const local of this function, which a mutable reference can name.
		bool changeable = false;
		// A variable of this function's own storage: a reference to thread memory binds only to it.
		bool inThread = false;
		// A place in the constant address space (a constant buffer, a file-scope constant, or a
		// constant reference parameter of this function): the only thing a constant reference binds.
		bool inConstant = false;
		// Names a pointer or a buffer that is not a local array: not lowered as an argument.
		bool pointerLike = false;
	};
	OverloadArgument classifyArgument(const Expression& argument);
	// Qualification orders two references or two pointers of one conversion rank (the one to a
	// non-const object is better); -1 where it does not apply.
	struct ConversionCost { int rank = kNoConversion; int qualification = -1; };
	ConversionCost conversionCost(const OverloadArgument& argument, const Parameter& parameter);
	std::string linkageName(const FunctionDecl& helper) const {
		return _overloadedNames.count(helper.name) ? helperKey(helper, _overloadedNames) : helper.name;
	}
	Id helperFunctionId(HelperFunction& helper);
	std::pair<Stage, const FunctionDecl*> helperScope(const HelperFunction& helper) const;
	void emitLibraryHelpers();
	void emitImports();
	bool isExported(const FunctionDecl& helper) const;
	bool isMemberName(const std::string& name) const {
		const size_t tail = name.rfind("::");
		return tail != std::string::npos && _unit.findStruct(name.substr(0, tail));
	}
	void linkageAttributes(Id function, const std::string& name, uint32_t type);
	Id helperParameterType(const Type& type);
	void rejectAliasedReferenceArguments(const Expression& call, const FunctionDecl& definition,
		size_t first);
	HelperFunction& pointerSpecialisation(const Expression& call, HelperFunction& generic,
		size_t first, std::vector<Id>& arrays);
	// The pointer value a device or constant pointer argument passes: the whole
	// buffer its name binds, checked against the parameter's address space.
	Id bufferPointerArgument(const Expression& argument, const Parameter& parameter,
		const std::string& what);
	Id referenceArgument(const Expression& argument, const Parameter& parameter,
		const std::string& function, size_t position, std::vector<std::pair<Id, Id>>& copyOuts);
	Id resourceArgument(const Expression& argument, const Parameter& parameter,
		const std::string& function);
	Id resourcePointee(const Type& type);
	void emitHelper(const HelperFunction& helper);
	void emitHelperReturn(const Statement& statement);
	void emitVoidReturnValue(const Expression& expression, const std::string& quoted);
	bool emitVoidCall(const Expression& call, const std::string* returnedBy);
	void emitVoidConditional(const Expression& expression, const std::string* returnedBy);
	bool returnsVoid(const FunctionDecl& function) const;
	const Type& returnTypeOf(const FunctionDecl& function) const;
	void deduceReturnType(HelperFunction& helper);
	Type deducedTypeOf(const Expression& returned, Id value, const std::string& quoted);
	void flushDeferredFunctions();
	Id emitRoundHalfAway(Id value, Id type, uint32_t bits);
	Id emitSignExact(Id value, Id type, uint32_t bits);
	Id atomicAddress(const Expression& argument, const std::string& function, bool& isSigned);
	Id emitAtomicCall(const Expression& expression);
	Id emitAtomicCompareExchange(const Expression& expression);
	Id emitSelectCall(const Expression& expression);
	Id emitAllAnyCall(const Expression& expression, const std::string& name);
	void emitBarrierCall(const Expression& expression);
	// floatVariant: the qualified name of a metal::fast or metal::precise call, whose arguments are
	// converted to float (see floatVariantResult).
	Id emitMathBuiltin(const MathBuiltin& builtin, const std::vector<ExpressionPtr>& arguments,
		const std::string* floatVariant = nullptr);
	Id floatVariantResult(Id value, const std::string& function);
	Id emitPackBuiltin(const PackBuiltin& builtin, const std::vector<ExpressionPtr>& arguments);
	Id packFloatConstant(Id type, float value);
	Id packConstantVector(ScalarKind kind, const std::vector<uint32_t>& words);
	Id glslInstruction(uint32_t instruction, Id type, const std::vector<Id>& operands);
	Id normalizedQuotient(Id magnitudes, uint32_t period, uint32_t count);
	Id emitConstruct(const Expression& expression);
	Id emitConstructList(const Expression& expression, Id toType);
	Id emitMatrixFromValue(const Expression& argument, const Type& target, Id toType,
		const std::string& spelled);
	Id emitInitListValue(const Type& target, const Expression& list);
	struct InitPlan;
	struct InitSlot {
		size_t index = 0;
		const InitializerElement* element = nullptr;
		std::shared_ptr<InitPlan> group;
	};
	struct InitPlan {
		const StructDecl* decl = nullptr;
		std::vector<InitSlot> slots;
	};
	struct DroppedInit {
		InitSlot slot;
		const Type* field;
	};
	void planStructElements(const StructDecl& decl, const std::vector<InitializerElement>& elements,
		size_t& cursor, std::vector<std::optional<Id>>& values, bool elided, InitPlan& plan,
		std::vector<DroppedInit>& dropped);
	Id emitInitSlot(const Type& field, const InitSlot& slot, std::vector<std::optional<Id>>& values);
	Id emitInitPlan(const Type& target, const InitPlan& plan, std::vector<std::optional<Id>>& values);
	Id emitStructElements(const Type& target, const StructDecl& decl, const std::vector<InitializerElement>& elements,
		std::vector<std::optional<Id>>& values);
	void discardLifted(const std::function<void()>& emit);
	Id emitListScalarValue(const Type& target, const Expression& expression, bool dropExcess = false);
	void checkAndDiscard(const Expression& expression);
	Id convertListScalar(Id value, const Type& target, const Expression& expression);
	bool canFoldExpression(const Expression& expression, const std::string& excluded = {});
	const StructDecl* structOf(const Expression& expression);
	Id emitSwizzle(const Expression& expression);
	Id emitIdentifier(const Expression& expression);
	Id loadFrom(Id pointer, Id pointeeType);
	Id promotedTo(uint32_t components) const;
	Id convert(Id value, Id fromType, Id toType);
	Id convertArray(Id value, Id fromType, Id toType);
	Id convertStruct(Id value, const TypeTable::StructForm& from, const TypeTable::StructForm& to, Id toType);
	Id composeStruct(Id type, const std::vector<uint32_t>& parts);
	Id constantStruct(Id type, const std::vector<uint32_t>& parts);
	Id convertImplicit(Id value, Id toType);
	Id asCondition(Id value);
	Id emitCondition(const Expression& expression);
	Id broadcast(Id value, Id vectorType);
};

} // namespace mslc
