#include "parser/internal.h"

namespace mslc {

StatementPtr Parser::parseIfStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::If;
	statement->line = line();

	expectKeyword("if", "at the start of an if");
	expect(TokenKind::LParen, "after 'if'");
	statement->expression = parseExpression();
	expect(TokenKind::RParen, "after an if condition");

	statement->thenBranch = parseBody();

	if (matchIdentifier("else")) {
		statement->elseBranch = parseBody();
	}

	return statement;
}

StatementPtr Parser::parseForStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::For;
	statement->line = line();
	const LocalScope locals(*this);

	expectKeyword("for", "at the start of a for");
	expect(TokenKind::LParen, "after 'for'");

	std::vector<StatementPtr> extraDeclarators;
	{
		Type probe;
		size_t nameTokens = 0;
		const std::string name = kind() == TokenKind::Identifier || at(TokenKind::ColonColon)
			? peekResolved(nameTokens) : std::string();
		// A local hides a struct tag of its own spelling only where it is not
		// qualified with "::" (j8, j9; see parseStatement's identical guard).
		const bool localHidesStruct = kind() == TokenKind::Identifier && lookahead().kind != TokenKind::ColonColon
			&& isLocal(name);
		size_t vecTokens = 0;
		const bool looksLikeType = (kind() == TokenKind::Identifier
				&& (isIntegerSpecifier(current().text) || current().text == "auto"))
			|| ((kind() == TokenKind::Identifier || at(TokenKind::ColonColon))
				&& !objectHidesTag(name) && (resolveTypeName(name, probe) || (_unit.findStruct(name) != nullptr && !localHidesStruct)))
			|| atLibraryVec(vecTokens) || atTraitType(vecTokens);

		if (looksLikeType) {
			VariableDeclaration declaration;
			Type base;
			const size_t typeFrom = _position;
			declaration.type = parseType(false, true, &base);
			_localIndependent = typeTokensIndependent(typeFrom);
			_localInjected = namesInjectedClass(typeFrom);
			rejectLocalQualifiers(declaration.type, at(TokenKind::Ampersand));
			if (declaration.type.isAuto && (declaration.type.isPointer || declaration.type.arrayLength)) {
				throw CompileError("a pointer or an array declared with auto is not lowered yet");
			}
			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a loop variable name in a for initialiser");
			}
			declaration.name = std::string(advance().text);
			declareLocal(declaration.name);
			noteLocal(declaration.name);
			if (declaration.type.isAuto) {
				rejectParameterListInitializer(declaration);
				parseAutoInitializer(declaration);
			} else if (match(TokenKind::Assign)) {
				declaration.initializer = parseInitializer();
			} else if (at(TokenKind::LBrace)) {
				declaration.initializer = parseInitializerList();
			}
			recordObjectType(declaration.name, declaration.type);
			statement->forInitializer = std::move(declaration);

			// "for (int i = 0, j = 1; ...)" : the further declarators are declared
			// ahead of the loop, in a block of its own, so the names stay its own.
			while (at(TokenKind::Comma)) {
				if (base.isAuto) {
					throw CompileError("several declarators after auto are not lowered yet; Apple requires each "
						"to deduce the same type, so declare each variable on its own line");
				}
				advance();
				auto next = std::make_unique<Statement>();
				next->kind = StatementKind::DeclarationStatement;
				next->line = line();
				next->declaration = parseNextLocalDeclarator(base);
				extraDeclarators.push_back(std::move(next));
			}
		} else if (!at(TokenKind::Semicolon)) {
			statement->expression = parseExpression();
		}
	}

	expect(TokenKind::Semicolon, "after a for initialiser");

	if (!at(TokenKind::Semicolon)) {
		statement->forCondition = parseExpression();
	}
	expect(TokenKind::Semicolon, "after a for condition");

	if (!at(TokenKind::RParen)) {
		statement->forIncrement = parseExpression();
	}
	expect(TokenKind::RParen, "after a for header");

	statement->forBody = parseBody(true);

	if (extraDeclarators.empty()) {
		return statement;
	}

	auto block = std::make_unique<Statement>();
	block->kind = StatementKind::Compound;
	block->line = statement->line;
	auto first = std::make_unique<Statement>();
	first->kind = StatementKind::DeclarationStatement;
	first->line = statement->line;
	first->declaration = std::move(statement->forInitializer);
	statement->forInitializer.reset();
	block->children.push_back(std::move(first));
	for (StatementPtr& extra: extraDeclarators) {
		block->children.push_back(std::move(extra));
	}
	block->children.push_back(std::move(statement));
	return block;
}

StatementPtr Parser::parseWhileStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::While;
	statement->line = line();

	expectKeyword("while", "at the start of a while");
	expect(TokenKind::LParen, "after 'while'");
	statement->whileCondition = parseExpression();
	expect(TokenKind::RParen, "after a while condition");

	statement->whileBody = parseBody();

	return statement;
}

StatementPtr Parser::parseDoWhileStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::While;
	statement->isDoWhile = true;
	statement->line = line();

	expectKeyword("do", "at the start of a do statement");
	statement->whileBody = parseBody();
	expectKeyword("while", "after the body of a do statement");
	expect(TokenKind::LParen, "after 'while'");
	statement->whileCondition = parseExpression();
	expect(TokenKind::RParen, "after a do-while condition");
	expect(TokenKind::Semicolon, "after a do-while statement");

	return statement;
}

// A statement that does nothing: a null statement or a block of them.
static bool isEmptyStatement(const Statement& statement) {
	if (statement.kind == StatementKind::ExpressionStatement) {
		return !statement.expression && !statement.isFallthrough;
	}
	return statement.kind == StatementKind::Compound
		&& std::all_of(statement.children.begin(), statement.children.end(),
			[](const StatementPtr& child) { return isEmptyStatement(*child); });
}

// Control never reaches the statement after one of these, and Apple ignores a marker there
// (discard_fragment() does not count: Apple still checks what follows it).
static bool leavesTheBlock(const Statement& statement) {
	return statement.kind == StatementKind::Return || statement.kind == StatementKind::Break
		|| statement.kind == StatementKind::Continue;
}

[[noreturn]] static void rejectFallthroughPlacement() {
	throw CompileError("fallthrough annotation does not directly precede switch label");
}

// Whether a [[fallthrough]]; is anywhere in this statement, reachable or not.
static bool containsFallthrough(const Statement& statement) {
	switch (statement.kind) {
		case StatementKind::ExpressionStatement:
			return statement.isFallthrough;
		case StatementKind::Compound:
			return std::any_of(statement.children.begin(), statement.children.end(),
				[](const StatementPtr& child) { return containsFallthrough(*child); });
		case StatementKind::If:
			return containsFallthrough(*statement.thenBranch)
				|| (statement.elseBranch && containsFallthrough(*statement.elseBranch));
		case StatementKind::For:
			return containsFallthrough(*statement.forBody);
		case StatementKind::While:
			return containsFallthrough(*statement.whileBody);
		case StatementKind::Switch:
			return statement.switchFallsOut;
		default:
			return false;
	}
}

// What the statements after a jump have shown so far: a marker was seen (only the last marker
// of the run is ignored), and a marker was seen on the way to here (so what follows must flow
// plainly on to the label).
struct UnreachableRun {
	bool seenMarker = false;
	bool afterMarker = false;
};

// A statement after a jump. Apple ignores a marker there, except that only the last marker of
// such a run is ignored, it still has to reach the label through plain statements (no jump,
// loop, switch or if with an else after it), and a marker in a loop body is an error whether it
// is reached or not.
static void checkUnreachable(const Statement& statement, UnreachableRun& run) {
	const bool branches = statement.kind == StatementKind::For || statement.kind == StatementKind::While
		|| statement.kind == StatementKind::Switch
		|| (statement.kind == StatementKind::If && statement.elseBranch);
	if (run.afterMarker && (branches || leavesTheBlock(statement))) rejectFallthroughPlacement();
	switch (statement.kind) {
		case StatementKind::ExpressionStatement:
			if (statement.isFallthrough) {
				if (run.seenMarker) rejectFallthroughPlacement();
				run.seenMarker = run.afterMarker = true;
			}
			return;
		case StatementKind::Compound:
			for (const StatementPtr& child: statement.children) checkUnreachable(*child, run);
			return;
		case StatementKind::If: {
			// A jump in a branch leaves only that branch, so a branch starts without the
			// "after a marker" demand and the if as a whole carries a marker out of either.
			UnreachableRun inThen{ run.seenMarker, false };
			UnreachableRun inElse{ run.seenMarker, false };
			checkUnreachable(*statement.thenBranch, inThen);
			if (statement.elseBranch) checkUnreachable(*statement.elseBranch, inElse);
			run.seenMarker = inThen.seenMarker || inElse.seenMarker;
			run.afterMarker = run.afterMarker || inThen.afterMarker || inElse.afterMarker;
			return;
		}
		default:
			if (containsFallthrough(statement)) rejectFallthroughPlacement();
			return;
	}
}

// Whether a [[fallthrough]]; inside this statement is the last thing it does, so that
// control leaves the statement straight from the marker. A marker anywhere else, or in a
// loop (whose body goes back to the head, not on to a label), is an error.
static bool endsWithFallthrough(const Statement& statement) {
	const auto reject = rejectFallthroughPlacement;
	switch (statement.kind) {
		case StatementKind::ExpressionStatement:
			return statement.isFallthrough;
		case StatementKind::Compound: {
			bool ends = false;
			UnreachableRun run;
			bool jumped = false;
			for (const StatementPtr& child: statement.children) {
				if (jumped) {
					checkUnreachable(*child, run);
					continue;
				}
				if (ends && !isEmptyStatement(*child)) reject();
				if (leavesTheBlock(*child)) {
					jumped = true;
					continue;
				}
				ends = ends || endsWithFallthrough(*child);
			}
			return ends;
		}
		case StatementKind::If: {
			const bool inThen = endsWithFallthrough(*statement.thenBranch);
			const bool inElse = statement.elseBranch && endsWithFallthrough(*statement.elseBranch);
			return inThen || inElse;
		}
		case StatementKind::For:
			if (containsFallthrough(*statement.forBody)) reject();
			return false;
		case StatementKind::While:
			if (containsFallthrough(*statement.whileBody)) reject();
			return false;
		case StatementKind::Switch:
			// A marker ending the last case of this switch falls out of it.
			return statement.switchFallsOut;
		default:
			return false;
	}
}

// A switch parses into a flat list of labels, each holding the statements up
// to the next label: the block structure the lowering wants is one block per
// label, branching to the next for a fall-through. Statements before the first
// label are reachable from nowhere, as C++ says, and are kept in the preamble.
StatementPtr Parser::parseSwitchStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::Switch;
	statement->line = line();
	const LocalScope locals(*this);

	expectKeyword("switch", "at the start of a switch");
	expect(TokenKind::LParen, "after 'switch'");
	statement->expression = parseExpression();
	expect(TokenKind::RParen, "after a switch selector");

	// C++ takes any statement for a body, but a case label can only live inside
	// a block, so the block is the only body that holds a switch's meaning.
	if (!at(TokenKind::LBrace)) {
		throw CompileError("a switch body needs braces around its case labels");
	}
	advance();

	struct DepthGuard {
		int& depth;
		explicit DepthGuard(int& value): depth(value) { ++depth; }
		~DepthGuard() { --depth; }
	} depthGuard(_switchDepth);

	SwitchCase* kase = nullptr;
	bool markerPending = false;
	bool unreachable = false;
	UnreachableRun unreachableRun;
	while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
		if (atKeyword("case")) {
			advance();
			SwitchCase next;
			next.line = line();
			next.value = parseAssignment();
			expect(TokenKind::Colon, "after a case value");
			statement->switchCases.push_back(std::move(next));
			kase = &statement->switchCases.back();
			markerPending = false;
			unreachable = false;
			unreachableRun = UnreachableRun();
			continue;
		}
		if (atKeyword("default")) {
			advance();
			SwitchCase next;
			next.line = line();
			expect(TokenKind::Colon, "after 'default'");
			statement->switchCases.push_back(std::move(next));
			kase = &statement->switchCases.back();
			markerPending = false;
			unreachable = false;
			unreachableRun = UnreachableRun();
			continue;
		}

		// A [[fallthrough]]; has to reach the next label with nothing in between, as Apple
		// requires: it may end a nested block or an if branch, but not be followed by a
		// statement, and a loop body never reaches a label. Null statements and empty
		// blocks do not count.
		std::vector<StatementPtr>& into = kase ? kase->body : statement->switchPreamble;
		const size_t before = into.size();
		appendStatement(into);
		for (size_t index = before; index < into.size(); ++index) {
			const Statement& added = *into[index];
			// Nothing after a jump, and nothing before the first label, is reached.
			if (!kase) continue;
			if (unreachable) {
				checkUnreachable(added, unreachableRun);
				continue;
			}
			if (markerPending && !isEmptyStatement(added)) {
				throw CompileError("fallthrough annotation does not directly precede switch label");
			}
			if (leavesTheBlock(added)) {
				unreachable = true;
				continue;
			}
			markerPending = markerPending || endsWithFallthrough(added);
		}
	}

	// In a nested switch the marker falls out to whatever follows the switch, which the
	// enclosing switch checks; in the outermost one no label can follow it.
	if (markerPending && _switchDepth == 1) {
		throw CompileError("fallthrough annotation does not directly precede switch label");
	}
	statement->switchFallsOut = markerPending;
	expect(TokenKind::RBrace, "to close a switch body");

	return statement;
}

StatementPtr Parser::parseReturnStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::Return;
	statement->line = line();

	expectKeyword("return", "at the start of a return");

	if (!at(TokenKind::Semicolon)) {
		// "return {1, 2, 3};" copy-initialises the returned value from a braced list.
		statement->expression = at(TokenKind::LBrace) ? parseInitializerList() : parseExpression();
	}

	expect(TokenKind::Semicolon, "after a return");

	return statement;
}

} // namespace mslc
