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