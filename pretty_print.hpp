#pragma once

#include "clang-c/Index.h"
#include <string>
#include <iostream>

inline void pretty_print(CXCursor cursor, std::string indent) {
	CXCursorKind cursor_kind = clang_getCursorKind(cursor);
	CXType cursor_type = clang_getCursorType(cursor);
	CXString cursor_spelling = clang_getCursorSpelling(cursor);
	CXString cursor_kind_spelling = clang_getCursorKindSpelling(cursor_kind);
	CXString kind_spelling = clang_getTypeKindSpelling(cursor_type.kind);
	CXString display = clang_getCursorDisplayName(cursor);
	CXString display_ref = clang_getCursorDisplayName(clang_getCursorReferenced(cursor));
	std::cout
		<< indent
		<< "Cursor "  << clang_getCString(cursor_spelling)
		<< " Display "  << clang_getCString(display)
		<< " Display Ref "  << clang_getCString(display_ref)
		<< " CursorKind: " << clang_getCString(cursor_kind_spelling)
		<< " TypeKind: " << cursor_type.kind << " " << clang_getCString(kind_spelling)
		<< "\n";
	{
		CXString pretty = clang_getCursorUSR (cursor);
		std::cout << indent << "USR " << clang_getCString(pretty) << "\n";
		clang_disposeString(pretty);
	}
	clang_disposeString(cursor_spelling);
	clang_disposeString(kind_spelling);
	clang_disposeString(cursor_kind_spelling);
	clang_disposeString(display);
	clang_disposeString(display_ref);
}

inline
std::string
ninja_name(
	std::string name
) {
	std::string result = "";
	for (int i = 0; i < name.length(); i++) {
		if (
			name[i] == ':' || name[i] == '$'
			|| name[i] == ' ' || name[i] == '|'
			|| name[i] == '#' || name[i] == '@'
			|| name[i] == '(' || name[i] == '[' || name[i] == '}'
			|| name[i] == ')' || name[i] == ']' || name[i] == '{'
			|| name[i] == ' ' || name[i] == ','
		) {
			result += "_";
		}
		else if (name[i] == '.') {
			result += "dot";
		} else if (name[i] == '*') {
			result += "star";
		} else if (name[i] == '!') {
			result += "excl";
		} else if (name[i] == '+') {
			result += "plus";
		} else if (name[i] == '-') {
			result += "minus";
		} else if (name[i] == '^') {
			result += "caret";
		} else if (name[i] == '>') {
			result += "gt";
		} else if (name[i] == '<') {
			result += "lt";
		} else if (name[i] == '=') {
			result += "eq";
		} else if (name[i] == '&') {
			result += "amp";
		} else if (name[i] == '/') {
			result += "div";
		} else if(name[i] == '\\') {
			result += "bs";
		}else if (name[i] == '~') {
			result += "tilde";
		} else  {
			result += name[i];
		}
	}
	return result;
}
