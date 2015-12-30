﻿/*
	© 2012-2015 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NPLContext.cpp
\ingroup Adaptor
\brief NPL 上下文。
\version r1553
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 329 。
\par 创建时间:
	2012-08-03 19:55:29 +0800
\par 修改时间:
	2015-12-31 01:09 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NPL::NPLContext
*/


#include "NPLContext.h"
#include YFM_NPL_SContext
#include <ystdex/container.hpp>
#include <iostream>
#include YFM_NPL_NPLA1

namespace NPL
{

NPLContext::NPLContext(const FunctionMap& m)
	: Root(), Map(m), token_list(), sem()
{}

void
NPLContext::Eval(const string& arg)
{
	if(CheckLiteral(arg) == '\'')
		NPLContext(Map).Perform(ystdex::get_mid(arg));
}

ValueObject
NPLContext::FetchValue(const ValueNode& ctx, const string& name)
{
	if(auto p_id = LookupName(ctx, name))
		return p_id->Value;
	return ValueObject();
}

const ValueNode*
NPLContext::LookupName(const ValueNode& ctx, const string& id)
{
	TryRet(&ctx.at(id))
	CatchIgnore(std::out_of_range&)
	CatchIgnore(ystdex::bad_any_cast&)
	return {};
}

void
NPLContext::HandleIntrinsic(const string& cmd)
{
	if(cmd == "exit")
		throw SSignal(SSignal::Exit);
	if(cmd == "cls")
		throw SSignal(SSignal::ClearScreen);
	if(cmd == "about")
		throw SSignal(SSignal::About);
	if(cmd == "help")
		throw SSignal(SSignal::Help);
	if(cmd == "license")
		throw SSignal(SSignal::License);
}

#define NPL_TRACE 1

void
NPLContext::Reduce(size_t depth, const SemaNode& sema, const ContextNode& ctx,
	Continuation k)
{
	std::clog << "Depth = " << depth << ", Context = " << &ctx
		<< ", Semantics = " << &sema << std::endl;
	++depth;
	if(!sema.empty())
	{
		auto& cont(sema.GetContainerRef());
		const auto n(cont.size());

		if(n == 0)
			// NOTE: Empty list.
			sema.Value = ValueToken::Null;
		else if(n == 1)
		{
			// NOTE: List with single element shall be reduced to its value.
			Deref(cont.begin()).SwapContent(sema);
			Reduce(depth, sema, ctx, k);
		}
		else
		{
			// NOTE: List application: call by value.
			// FIXME: Context.
			for(auto& term : cont)
//PackNodes(sema.GetName(), ValueNode(0, "$parent", &ctx)
				Reduce(depth, term, ctx, k);
			ystdex::erase_all_if(cont, cont.begin(), cont.end(),
				[](const SemaNode& term){
					return !term;
				});
			if(cont.size() > 1)
				// NOTE: Match function calls.
				try
				{
					cont.begin()->Value.Access<FunctionHandler>()(sema, ctx);
					k(sema);
				}
				CatchExpr(std::out_of_range&, YTraceDe(Warning,
					"No matching functions found."))
				CatchThrow(ystdex::bad_any_cast& e, LoggedEvent(ystdex::sfmt(
					"Mismatched types <'%s', '%s'> found.", e.from(), e.to()),
					Warning))
			return;
		}
	}
	else if(sema.Value.AccessPtr<ValueToken>())
		;
	else if(auto p = sema.Value.AccessPtr<string>())
	{
		auto& id(*p);

		if(id == "," || id == ";")
			sema.Clear();
		else
		{
			HandleIntrinsic(id);
			// NOTE: Value rewriting.
			if(auto v = FetchValue(ctx, id))
				sema.Value = std::move(v);
		//	else
		//		throw LoggedEvent(ystdex::sfmt(
		//			"Wrong identifier '%s' found", id.c_str()), Warning);
		}
		k(sema);
	}
}

TokenList&
NPLContext::Perform(const string& unit)
{
	if(unit.empty())
		throw LoggedEvent("Empty token list found;", Alert);

	auto sema(SContext::Analyze(Session(unit)));

	Reduce(0, sema, Root, [](const SemaNode&){});
	// TODO: Merge result to 'Root'.

	using namespace std;

	PrintNode(cout, sema, LiteralizeEscapeNodeLiteral);
	cout << endl;

	return token_list;
}

} // namespace NPL;

