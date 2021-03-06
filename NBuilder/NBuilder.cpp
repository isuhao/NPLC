﻿/*
	© 2011-2017 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NBuilder.cpp
\ingroup NBuilder
\brief NPL 解释实现。
\version r6285
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 301
\par 创建时间:
	2011-07-02 07:26:21 +0800
\par 修改时间:
	2017-05-30 02:16 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::NBuilder
*/


#include "NBuilder.h"
#include <iostream>
#include <typeindex>
#include <Helper/YModules.h>
#include YFM_YSLib_Core_YApplication // for YSLib, Application;
#include YFM_NPL_Dependency // for NPL, NPL::A1, LoadNPLContextForSHBuild;

namespace NPL
{

namespace A1
{

void
RegisterLiteralSignal(ContextNode& node, const string& name, SSignal sig)
{
	RegisterLiteralHandler(node, name,
		[=](const ContextNode&) YB_ATTR(noreturn) -> ReductionStatus{
		throw sig;
	});
}

} // namespace A1;

} // namespace NPL;

using namespace NPL;
using namespace A1;
using namespace YSLib;
using namespace platform_ex;

namespace
{

/// 327
void
ParseOutput(LexicalAnalyzer& lex)
{
	const auto& cbuf(lex.GetBuffer());
	const auto xlst(lex.Literalize());
	using namespace std;
	const auto rlst(Tokenize(xlst));

	cout << "cbuf size:" << cbuf.size() << endl
		<< "xlst size:" << cbuf.size() << endl;
	for(const auto& str : rlst)
		cout << EncodeArg(str) << endl
			<< "* u8 length: " << str.size() << endl;
	cout << rlst.size() << " token(s) parsed." <<endl;
}

/// 737
void
ParseStream(std::istream& is)
{
	if(is)
	{
		Session sess;
		char c;

		while((c = is.get()), is)
			Session::DefaultParseByte(sess.Lexer, c);
		ParseOutput(sess.Lexer);
	}
}


/// 785
//@{
ReductionStatus
ProcessDebugCommand()
{
	string cmd;

	getline(std::cin, cmd);
	if(cmd == "r")
		return ReductionStatus::Retrying;
	return ReductionStatus::Clean;
}

bool use_debug = {};

ReductionStatus
DefaultDebugAction(TermNode& term)
{
	if(use_debug)
	{
		YTraceDe(Debug, "List term: %p", ystdex::pvoid(&term));
		LogTree(term);
		return ProcessDebugCommand();
	}
	return ReductionStatus::Clean;
}

ReductionStatus
DefaultLeafDebugAction(TermNode& term)
{
	if(use_debug)
	{
		YTraceDe(Debug, "Leaf term: %p", ystdex::pvoid(&term));
		LogTree(term);
		return ProcessDebugCommand();
	}
	return ReductionStatus::Clean;
}
//@}


/// 780
//@{
bool
IfDef(observer_ptr<const string> p, const ContextNode& ctx)
{
	return ystdex::call_value_or([&](string_view id){
		return bool(ResolveName(ctx, id));
	}, p);
}

void
LoadExternal(REPLContext& context, const string& name)
{
	platform::ifstream ifs(name, std::ios_base::in);

	if(ifs)
	{
		YTraceDe(Notice, "Test unit '%s' found.", name.c_str());
		FilterExceptions([&]{
			context.LoadFrom(ifs);
		});
	}
	else
		YTraceDe(Notice, "Test unit '%s' not found.", name.c_str());
}
//@}


/// 740
void
LoadFunctions(REPLContext& context)
{
	using namespace std::placeholders;
	using namespace Forms;
	auto& root(context.Root);
	auto& root_env(root.GetRecordRef());

	LoadNPLContextForSHBuild(context);
	// TODO: Extract literal configuration API.
//	LoadSequenceSeparators(root, context.ListTermPreprocess);
//	LoadDeafultLiteralPasses(root);
	// NOTE: Literal builtins.
	RegisterLiteralSignal(root, "exit", SSignal::Exit);
	RegisterLiteralSignal(root, "cls", SSignal::ClearScreen);
	RegisterLiteralSignal(root, "about", SSignal::About);
	RegisterLiteralSignal(root, "help", SSignal::Help);
	RegisterLiteralSignal(root, "license", SSignal::License);
	// NOTE: Definition of %inert is in %YFramework.NPL.Dependency.
	// NOTE: Context builtins.
	root_env.Define("REPL-context", ValueObject(context, OwnershipTag<>()), {});
	root_env.Define("root-context", ValueObject(root, OwnershipTag<>()), {});
	// NOTE: Literal expression forms.
	RegisterForm(root, "$retain", Retain);
	RegisterForm(root, "$retain1", ystdex::bind1(RetainN, 1));
#if true
	// NOTE: Primitive features, listed as RnRK, except mentioned above. See
	//	%YFramework.NPL.Dependency.
	RegisterStrict(root, "eq?", EqualReference);
//	RegisterStrict(root, "eqv?", EqualValue);
	// FIXME: This requires a string.
	RegisterStrictUnary<const string>(root, "symbol?", IsSymbol);
	// NOTE: Like Scheme but not Kernel, %'$if' treats non-boolean value as
	//	'#f', for zero overhead principle.
//	RegisterForm(root, "$if", If);
	RegisterStrictUnary(root, "list?", IsList);
	// TODO: Add nonnull list predicate to improve performance?
	// NOTE: Definitions of null?, cons, eval, get-current-environment,
	//	copy-environment, make-environment are in %YFramework.NPL.Dependency.
	// NOTE: Environment mutation is optional in Kernel and supported here.
	// NOTE: Definitions of $deflazy!, $def!, $defrec! are in
	//	%YFramework.NPL.Dependency.
	RegisterForm(root, "$undef!", ystdex::bind1(Undefine, _2, true));
	RegisterForm(root, "$undef-checked!", ystdex::bind1(Undefine, _2, false));
//	RegisterForm(root, "$vau", Vau);
	RegisterForm(root, "$vaue", VauWithEnvironment);
//	RegisterStrictUnary<ContextHandler>(root, "wrap", Wrap);
	// NOTE: This does check before wrapping.
	RegisterStrictUnary<ContextHandler>(root, "wrap1", WrapOnce);
//	RegisterStrictUnary<ContextHandler>(root, "unwrap", Unwrap);
	// NOTE: 'eq? (() get-current-environment) (() (wrap ($vau () e e)))' shall
	//	be '#t'.
#endif
	// NOTE: NPLA value transferring.
	RegisterStrictUnary(root, "vcopy", [](TermNode& term){
		return term.Value.MakeCopy();
	});
	RegisterStrictUnary(root, "vcopymove", [](TermNode& term){
		return term.Value.CopyMove();
	});
	RegisterStrictUnary(root, "vmove", [](TermNode& term){
		return term.Value.MakeMove();
	});
	RegisterStrictUnary(root, "vmovecopy", [](TermNode& term){
		return term.Value.MakeMoveCopy();
	});
	// XXX: Unsafe.
	RegisterStrictUnary(root, "vref", ystdex::compose(ReferenceValue,
		ystdex::bind1(std::mem_fn(&TermNode::Value))));
	// XXX: For test or debug only.
	RegisterStrictUnary(root, "tt", DefaultDebugAction);
	RegisterStrictUnary<const string>(root, "dbg", [](const string& cmd){
		if(cmd == "on")
			use_debug = true;
		else if(cmd == "off")
			use_debug = {};
		else if(cmd == "crash")
			terminate();
	});
	RegisterForm(root, "$crash", []{
		terminate();
	});
	// NOTE: Definitions of $sequence, list are in %YFramework.NPL.Dependency.
#if true
	RegisterStrict(root, "id", [](TermNode& term){
		RetainN(term);
		LiftTerm(term, Deref(std::next(term.begin())));
		return
			IsBranch(term) ? ReductionStatus::Retained : ReductionStatus::Clean;
	});
#else
	context.Perform(u8R"NPL($def! id $lambda (x) x)NPL");
#endif
	// NOTE: Definitions of first, rest are in %YFramework.NPL.Dependency.
	context.Perform(u8R"NPL(
		$def! $quote $vau (x) #ignore x;
	)NPL");
	// NOTE: Definitions of apply, list*, $cond, $set!, $defl!, $defv!, $when,
	//	$unless, not? are in %YFramework.NPL.Dependency.
	// NOTE: Derived functions with privmitive implementation.
	// NOTE: Definitions of $and?, $or? are in %YFramework.NPL.Dependency.
	RegisterForm(root, "$delay", [](TermNode& term, ContextNode&){
		term.Remove(term.begin());

		ValueObject x(DelayedTerm(std::move(term)));

		term.Value = std::move(x);
		return ReductionStatus::Clean;
	});
	// NOTE: Definitions of ref is in %YFramework.NPL.Dependency.
	// TODO: Provide 'equal?'.
	RegisterForm(root, "evalv",
		static_cast<void(&)(TermNode&, ContextNode&)>(ReduceChildren));
	RegisterStrict(root, "force", [](TermNode& term){
		RetainN(term);
		return EvaluateDelayed(term,
			Access<DelayedTerm>(Deref(std::next(term.begin()))));
	});
	// NOTE: Comparison.
	RegisterStrictBinary<int>(root, "=?", ystdex::equal_to<>());
	RegisterStrictBinary<int>(root, "<?", ystdex::less<>());
	RegisterStrictBinary<int>(root, "<=?", ystdex::less_equal<>());
	RegisterStrictBinary<int>(root, ">=?", ystdex::greater_equal<>());
	RegisterStrictBinary<int>(root, ">?", ystdex::greater<>());
	// NOTE: Arithmetic procedures.
	// FIXME: Overflow?
	RegisterStrict(root, "+", std::bind(CallBinaryFold<int, ystdex::plus<>>,
		ystdex::plus<>(), 0, _1));
	// FIXME: Overflow?
	RegisterStrictBinary<int>(root, "add2", ystdex::plus<>());
	// FIXME: Underflow?
	RegisterStrictBinary<int>(root, "-", ystdex::minus<>());
	// FIXME: Overflow?
	RegisterStrict(root, "*", std::bind(CallBinaryFold<int,
		ystdex::multiplies<>>, ystdex::multiplies<>(), 1, _1));
	// FIXME: Overflow?
	RegisterStrictBinary<int>(root, "multiply2", ystdex::multiplies<>());
	RegisterStrictBinary<int>(root, "/", [](int e1, int e2){
		if(e2 != 0)
			return e1 / e2;
		throw std::domain_error("Runtime error: divided by zero.");
	});
	RegisterStrictBinary<int>(root, "%", [](int e1, int e2){
		if(e2 != 0)
			return e1 % e2;
		throw std::domain_error("Runtime error: divided by zero.");
	});
	// NOTE: I/O library.
	RegisterStrictUnary<const string>(root, "ofs", [&](const string& path){
		if(ifstream ifs{path})
			return ifs;
		throw LoggedEvent(
			ystdex::sfmt("Failed opening file '%s'.", path.c_str()));
	});
	RegisterStrictUnary<const string>(root, "oss", [&](const string& str){
		return std::istringstream(str);
	});
	RegisterStrictUnary<ifstream>(root, "parse-f", ParseStream);
	RegisterStrictUnary<LexicalAnalyzer>(root, "parse-lex", ParseOutput);
	RegisterStrictUnary<std::istringstream>(root, "parse-s", ParseStream);
	RegisterStrictUnary<const string>(root, "put", [&](const string& str){
		std::cout << EncodeArg(str);
	});
	RegisterStrictUnary<const string>(root, "puts", [&](const string& str){
		// XXX: Overridding.
		std::cout << EncodeArg(str) << std::endl;
	});
	// NOTE: Interoperation library.
	RegisterStrict(root, "display", ystdex::bind1(LogTree, Notice));
	RegisterStrictUnary<const string>(root, "echo", Echo);
	// NOTE: Environments.
	RegisterForm(root, "$binds?",
		[](TermNode& term, const ContextNode& ctx){
		CallUnaryAs<const TokenValue>([&](const TokenValue& t){
			return IfDef(make_observer(&t), ctx);
		}, term);
	});
	// NOTE: Following strict variants are not in Kernel.
	RegisterStrictUnary(root, "binds?",
		[](TermNode& term, const ContextNode& ctx){
		return IfDef(AccessPtr<string>(term), ctx);
	});
	RegisterStrictBinary(root, "binds-in?", [&](TermNode& t, TermNode& c){
		return IfDef(AccessPtr<string>(t), Access<ContextNode>(c));
	});
	RegisterStrict(root, "eval-u",
		ystdex::bind1(EvaluateUnit, std::ref(context)));
	RegisterStrict(root, "eval-u-in", [](TermNode& term){
		const auto i(std::next(term.begin()));
		const auto& rctx(Access<REPLContext>(Deref(i)));

		term.Remove(i);
		EvaluateUnit(term, rctx);
	}, ystdex::bind1(RetainN, 2));
	RegisterStrictUnary<const string>(root, "lex", [&](const string& unit){
		LexicalAnalyzer lex;

		for(const auto& c : unit)
			lex.ParseByte(c);
		return lex;
	});
	RegisterStrictUnary<const std::type_index>(root, "nameof",
		[](const std::type_index& ti){
		return string(ti.name());
	});
	// NOTE: Type operation library.
	RegisterStrictUnary(root, "typeid", [](TermNode& term){
		// FIXME: Get it work with %YB_Use_LightweightTypeID.
		return std::type_index(term.Value.GetType());
	});
	context.Perform("$defl! ptype (x) puts (nameof (typeid(x)))");
	RegisterStrictUnary<string>(root, "get-typeid",
		[&](const string& str) -> std::type_index{
		if(str == "bool")
			return typeid(bool);
		if(str == "environment")
			// XXX: The environment type is not unique.
			return typeid(weak_ptr<NPL::Environment>);
		if(str == "int")
			return typeid(int);
		if(str == "operative")
			return typeid(FormContextHandler);
		if(str == "applicative")
			return typeid(StrictContextHandler);
		if(str == "string")
			return typeid(string);
		return typeid(void);
	});
	context.Perform("$defl! bool? (x) eqv? (get-typeid \"bool\")"
		" (typeid x)");
	context.Perform("$defl! environment? (x) eqv? (get-typeid \"environment\")"
		" (typeid x)");
	// FIXME: Wrong result.
	context.Perform("$defl! operative? (x) eqv? (get-typeid \"operative\")"
		" (typeid x)");
	// FIXME: Wrong result.
	context.Perform("$defl! applicative? (x) eqv? (get-typeid \"applicative\")"
		" (typeid x)");
	context.Perform("$defl! int? (x) eqv? (get-typeid \"int\")"
		" (typeid x)");
	context.Perform("$defl! string? (x) eqv? (get-typeid \"string\")"
		" (typeid x)");
	// NOTE: List library.
	RegisterStrictUnary(root, "list-length", [&](TermNode& term){
		return int(term.size());
	});
	// NOTE: String library.
	RegisterStrict(root, "string=?", std::bind(CallBinaryFold<string,
		ystdex::equal_to<>>, ystdex::equal_to<>(), string(), _1));
	context.Perform(u8R"NPL($defl! retain-string (str) ++ "\"" str "\"")NPL");
	RegisterStrictUnary<const int>(root, "itos", [](int x){
		return to_string(x);
	});
	RegisterStrictUnary<const string>(root, "string-length",
		[&](const string& str){
		return int(str.length());
	});
	// NOTE: Definitions of string->symbol, symbol->string are in
	//	%YFramework.NPL.Dependency.
	// NOTE: SHBuild builitins.
	// XXX: Overriding.
	root_env.Define("SHBuild_BaseTerminalHook_",
		ValueObject(std::function<void(const string&, const string&)>(
		[](const string& n, const string& val){
			// XXX: Errors from stream operations are ignored.
			using namespace std;
			Terminal te;

			cout << te.LockForeColor(DarkCyan) << n;
			cout << " = \"";
			cout << te.LockForeColor(DarkRed) << val;
			cout << '"' << endl;
	})), true);
	context.Perform("$defl! iput (x) puts (itos x)");
	RegisterStrictUnary<const string>(root, "load",
		std::bind(LoadExternal, std::ref(context), _1));
	LoadExternal(context, "test.txt");
	AccessListPassesRef(root).Add(DefaultDebugAction, 255);
	AccessLeafPassesRef(root).Add(DefaultLeafDebugAction, 255);
}

} // unnamed namespace;


/// 304
int
main(int argc, char* argv[])
{
	using namespace std;

	// XXX: Windows 10 rs2_prerelease may have some bugs for MBCS console
	//	output. String from 'std::putc' or iostream with stdio buffer (unless
	//	set with '_IONBF') seems to use wrong width of characters, resulting
	//	mojibake. This is one of the possible workaround. Note that
	//	'ios_base::sync_with_stdio({})' would also fix this problem but it would
	//	disturb prompt color setting.
	ystdex::setnbuf(stdout);
	yunused(argc), yunused(argv);
	return FilterExceptions([]{
		Application app;
		Interpreter intp(app, LoadFunctions);

		while(intp.WaitForLine() && intp.Process())
			;
	}, "::main") ? EXIT_FAILURE : EXIT_SUCCESS;
}

