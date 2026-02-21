// main.cpp
#include"clang-c/Index.h"
#include <iostream>
#include <vector>
#include <string>

enum class link_status {
	await_target, read_target,
	read_input, read_input_2
};

struct link {
	link_status state;
	bool active;
	std::string target;
	std::vector<std::string> inputs;
};


int main(){

	// "-mavx2", "-mfma",

	std::vector<char*> new_flags { "clang",
		"-cc1 -triple x86_64-pc-windows-msvc19.50.35724 -emit-obj", "-mincremental-linker-compatible", "-disable-free", "-clear-ast-before-backend", "-disable-llvm-verifier", "-discard-value-names", "-main-file-name economy.cpp", "-mrelocation-model pic", "-pic-level 2", "-mframe-pointer=none", "-relaxed-aliasing", "-fmath-errno", "-ffp-contract=on", "-fno-rounding-math", "-mconstructor-aliases", "-fms-volatile", "-funwind-tables=2", "-target-cpu x86-64", "-target-feature +avx2",
"-tune-cpu generic", "-fdebug-compilation-dir=C:/gamedev/012", "-v" "-fcoverage-compilation-dir=C:/gamedev/012", "-resource-dir C:/Program Files/LLVM/lib/clang/21", "-D GLEW_NO_GLU", "-D GLEW_STATIC", "-D GLM_ENABLE_EXPERIMENTAL", "-D INCREMENTAL=1", "-D PROJECT_ROOT=C:/_projects/cpp/alice", "-D VC_EXTRALEAN", "-D WIN32_MEAN_AND_LEAN", "-D _CRT_SECURE_NO_WARNINGS", "-IC:/_projects/cpp/alice/src", "-IC:/_projects/cpp/alice/src/ai", "-IC:/_projects/cpp/alice/src/common_types", "-IC:/_projects/cpp/alice/src/filesystem", "-IC:/_projects/cpp/alice/src/gamestate", "-IC:/_projects/cpp/alice/src/gui", "-IC:/_projects/cpp/alice/src/gui/province_tiles", "-IC:/_projects/cpp/alice/src/gui/topbar_subwindows", "-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/diplomacy_subwindows", "-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/production_subwindows", "-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/politics_subwindows", "-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/military_subwindows", "-IC:/_projects/cpp/alice/src/graphics", "-IC:/_projects/cpp/alice/src/parsing", "-IC:/_projects/cpp/alice/src/window", "-IC:/_projects/cpp/alice/src/text", "-IC:/_projects/cpp/alice/src/sound", "-IC:/_projects/cpp/alice/src/map", "-IC:/_projects/cpp/alice/src/network", "-IC:/_projects/cpp/alice/src/nations", "-IC:/_projects/cpp/alice/src/gamerule", "-IC:/_projects/cpp/alice/src/provinces", "-IC:/_projects/cpp/alice/src/economy", "-IC:/_projects/cpp/alice/src/culture", "-IC:/_projects/cpp/alice/src/military", "-IC:/_projects/cpp/alice/src/scripting", "-IC:/_projects/cpp/alice/src/scripting/luajit", "-IC:/_projects/cpp/alice/src/zstd", "-IC:/_projects/cpp/alice/src/lunasvg", "-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/glew-src/include/GL", "-IC:/_projects/cpp/alice/ankerl", "-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/glew-src/include", "-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/freetype-build/include", "-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/freetype-src/include", "-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/harfbuzz-src/src", "-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/glm-src", "-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/datacontainer-src/CommonIncludes", "-IC:/_projects/cpp/alice/dependencies/stb/.", "-internal-isystem C:/Program Files/LLVM/lib/clang/21/include", "-internal-isystem F:/VisualStudio2026/VC/Tools/MSVC/14.50.35717/include", "-internal-isystem F:/VisualStudio2026/VC/Tools/MSVC/14.50.35717/atlmfc/include", "-internal-isystem C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/ucrt", "-internal-isystem C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/shared", "-internal-isystem C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/um", "-internal-isystem C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/winrt", "-internal-isystem C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/cppwinrt", "-Wno-nontrivial-memcall", "-std=c++20", "-fdeprecated-macro", "-ferror-limit 19", "-fno-use-cxa-atexit", "-fms-extensions", "-fms-compatibility", "-fms-compatibility-version=19.50.35724", "-fno-implicit-modules", "-fskip-odr-check-in-gmf", "-fcxx-exceptions", "-fexceptions", "-faddrsig"
	};

	std::vector<char*> flags {"clang++", "-std=c++23", "-mavx2", "-mfma", "-DGLEW_NO_GLU","-DGLEW_STATIC","-DGLM_ENABLE_EXPERIMENTAL","-DINCREMENTAL=1","-DPROJECT_ROOT=C:/_projects/cpp/alice","-DVC_EXTRALEAN","-DWIN32_MEAN_AND_LEAN","-D_CRT_SECURE_NO_WARNINGS","-IC:/_projects/cpp/alice/src","-IC:/_projects/cpp/alice/src/ai","-IC:/_projects/cpp/alice/src/common_types","-IC:/_projects/cpp/alice/src/filesystem","-IC:/_projects/cpp/alice/src/gamestate","-IC:/_projects/cpp/alice/src/gui","-IC:/_projects/cpp/alice/src/gui/province_tiles","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/diplomacy_subwindows","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/production_subwindows","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/politics_subwindows","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/military_subwindows","-IC:/_projects/cpp/alice/src/graphics","-IC:/_projects/cpp/alice/src/parsing","-IC:/_projects/cpp/alice/src/window","-IC:/_projects/cpp/alice/src/text","-IC:/_projects/cpp/alice/src/sound","-IC:/_projects/cpp/alice/src/map","-IC:/_projects/cpp/alice/src/network","-IC:/_projects/cpp/alice/src/nations","-IC:/_projects/cpp/alice/src/gamerule","-IC:/_projects/cpp/alice/src/provinces","-IC:/_projects/cpp/alice/src/economy","-IC:/_projects/cpp/alice/src/culture","-IC:/_projects/cpp/alice/src/military","-IC:/_projects/cpp/alice/src/scripting","-IC:/_projects/cpp/alice/src/scripting/luajit","-IC:/_projects/cpp/alice/src/zstd","-IC:/_projects/cpp/alice/src/lunasvg","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/glew-src/include/GL","-IC:/_projects/cpp/alice/ankerl","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/glew-src/include","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/freetype-build/include","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/freetype-src/include","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/harfbuzz-src/src","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/glm-src","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/datacontainer-src/CommonIncludes","-IC:/_projects/cpp/alice/dependencies/stb/.","-o economy.o"
	};


	CXIndex index = clang_createIndex(0, 0); //Create index
	CXTranslationUnit unit;

	clang_parseTranslationUnit2FullArgv(
		index,
		"C:/_projects/cpp/alice/src/economy/economy.cpp",
		flags.data(), flags.size(),
		nullptr, 0,
		CXTranslationUnit_None,
		&unit
	);

	if (unit == nullptr){
		std::cerr <<"Unable to parse translation unit. Quitting.\n";
		return 0;
	}


	auto diag_count = clang_getNumDiagnostics(unit);

	for (unsigned int i = 0; i < diag_count; i++) {
		auto  diag = clang_getDiagnostic(unit, i);
		auto text = clang_getDiagnosticSpelling(diag);
		std::cout << clang_getCString(text) <<"\n";
		clang_disposeString(text);
	}


	clang_saveTranslationUnit(unit,"dump.txt", CXSaveTranslationUnit_None);

	CXCursor cursor = clang_getTranslationUnitCursor(unit); //Obtain a cursor at the root of the translation unit

	auto pprint = clang_getCursorPrettyPrinted(cursor, NULL);

	std::cout << clang_getCString(pprint) <<"\n";

	clang_disposeString(pprint);

	/*
	clang_visitChildren(
		cursor, //Root cursor
		[](CXCursor current_cursor, CXCursor parent, CXClientData client_data){

		CXString current_display_name = clang_getCursorDisplayName(current_cursor);
		//Allocate a CXString representing the name of the current cursor

		std::cout <<"Visiting element" << clang_getCString(current_display_name) <<"\n";
		//Print the char* value of current_display_name

		clang_disposeString(current_display_name);
		//Since clang_getCursorDisplayName allocates a new CXString, it must be freed. This applies
		//to all functions returning a CXString

		return CXChildVisit_Recurse;


		}, //CXCursorVisitor: a function pointer
		nullptr //client_data
	);*/

	/*
	clang_visitChildren(
		cursor,
		[](CXCursor current_cursor, CXCursor parent, CXClientData client_data){
			CXType cursor_type = clang_getCursorType(current_cursor);

			CXString type_kind_spelling = clang_getTypeKindSpelling(cursor_type.kind);
			std::cout << current_cursor.xdata <<"\n";
			std::cout <<"TypeKind:" << clang_getCString(type_kind_spelling);
			clang_disposeString(type_kind_spelling);

			if (
				cursor_type.kind == CXType_Pointer
				|| cursor_type.kind == CXType_LValueReference
				|| cursor_type.kind == CXType_RValueReference
			) {
				CXType pointed_to_type = clang_getPointeeType(cursor_type);
				CXString pointed_to_type_spelling = clang_getTypeSpelling(pointed_to_type);     // Spell out the entire
				std::cout <<"pointing to type:" << clang_getCString(pointed_to_type_spelling);// pointed-to type
				clang_disposeString(pointed_to_type_spelling);
			} else if(cursor_type.kind == CXType_Record){
				CXString type_spelling = clang_getTypeSpelling(cursor_type);
				std::cout << ", namely" << clang_getCString(type_spelling);
				clang_disposeString(type_spelling);
			}
			std::cout <<"\n";
			return CXChildVisit_Recurse;
		},
		nullptr
	);
	*/

	link data;
	data.active = false;
	clang_visitChildren(
		cursor,
		[](CXCursor current_cursor, CXCursor parent, CXClientData client_data){

			link* in_data = (link*) client_data;

			CXString current_display_name = clang_getCursorDisplayName(current_cursor);
			auto current_cstr = clang_getCString(current_display_name);
			bool skip_children = false;
			if (current_cstr[0] == '=' && strlen(current_cstr) == 1) {
				// skip_children = true;
				std::cout <<"build ";
				in_data->state = link_status::await_target;

				clang_visitChildren(current_cursor, [](CXCursor c1, CXCursor c1_parent, CXClientData client_data_2){

					link* current_data = (link*) client_data_2;
					CXString parent_display_name = clang_getCursorDisplayName(c1_parent);
					auto parent_cstr = clang_getCString(parent_display_name);


					CXString c1_name = clang_getCursorDisplayName(c1);
					auto c1_cstr = clang_getCString(c1_name);

					if (parent_cstr[0] == '=' && strlen(parent_cstr) == 1 && current_data->state == link_status::await_target) {
						std::cout << c1_cstr <<" : compute ";
						current_data->state = link_status::read_input;
					} else {
						std::cout << c1_cstr << " ";
					}

					clang_disposeString(parent_display_name);
					return CXChildVisit_Recurse;
				}, client_data);

				std::cout <<"\n";
			}
			clang_disposeString(current_display_name);

			CXType cursor_type = clang_getCursorType(current_cursor);
			CXString cursor_spelling = clang_getCursorSpelling(current_cursor);
			CXSourceRange cursor_range = clang_getCursorExtent(current_cursor);
			CXString kind_spelling = clang_getTypeKindSpelling(cursor_type.kind);
			std::cout <<"Cursor " << clang_getCString(kind_spelling) << " " << clang_getCString(cursor_spelling);

			CXFile file;
			unsigned start_line, start_column, start_offset;
			unsigned end_line, end_column, end_offset;

			clang_getExpansionLocation(clang_getRangeStart(cursor_range), &file, &start_line, &start_column, &start_offset);
			clang_getExpansionLocation(clang_getRangeEnd  (cursor_range), &file, &end_line  , &end_column  , &end_offset);

			std::cout <<" spanning lines " << start_line <<" to" << end_line;

			clang_disposeString(cursor_spelling);
			clang_disposeString(kind_spelling);

			std::cout <<"\n";

			if (cursor_type.kind == CXType_Invalid) {
				return CXChildVisit_Recurse;
			}

			if (skip_children) {
				return CXChildVisit_Continue;
			}
			return CXChildVisit_Continue;
		},
		(CXClientData) &data
	);

	/*
	*/
}