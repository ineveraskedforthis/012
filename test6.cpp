#include "clang-c/Index.h"
#include <cassert>
#include <map>
#include <string>
#include <vector>
#include <optional>
#include <utility>
#include <map>

#include "pretty_print.hpp"

/*
Fix any set W. We will call it a "word space"
We say that an atomic word is an element of W.

W-memory device M of width N is a product of N copies of W. where N is a natural number.
We say that n-th copy of W in a memory device M is a "n-th word in M"
Explicit atomic variable in a W-memory device M of width N is a natural number from 0 to N - 1.
Elements of M could be called "memory states" or "memory configurations".

	Note:
	Sometimes explicit memory location could be found only during runtime.
	For example, when we index an array with variable.
	We say that in this situation variable has "implicit memory location".
	Theoretically, this index could be as computationally complex as the program itself.
	The best we can do in this case is to ignore the index altogether in analysis and treat all arrays as if they contain only one object.
	Variables with implicit memory location would have "variable" part which refers to this unique object and "parameters" part
	which specify memory location of the final indices.

K-word in M is an injective map from {0, ... , K - 1} to {0, ... , N - 1}
Morhism between K-words is a bijection between their images.

Define J(K) as a functor from the category of K-words into the category of multiplicands of W,
which maps K-word into the corresponding multiplicand of M
while mapping permutation morhisms into the permutation matrices

Define resolved (n,m,k)-instruction as a tuple (F, G, t) where
- F is a functor from n-words to m-words,
- G is a functor from n-words to k-words,
- t is a natural transformation from J(F(-)) to J(G(-)).


A statement for W-memory device M of width N is a tuple (n, m, k, a, q) where a is an n-word and q is a resolved (n, m, k) instruction.
Evaluation of a statement (n, m, k, a, (F, G, t)) is a map from M to M which is defined as follows:
If i-th cell is outside of cells G(a), then it preserves the original value
Values in G(a)-numbered cells are defined as image of F(a)-slice of the memory state under t(a).

Sequential resolved program is a sequence of statements.
Evaluation of program is a composition of sequential evaluations of statements of the program.

We say that two statements
(n1, m1, k1, a1, (F1, G1, t1))
(n2, m2, k2, a2, (F2, G2, t2))
surely do not conflict if the followings intersections are empty:
- G1(a1) and G2(a2) : instructions do not write in the same place
- G1(a1) and F2(a2) : the second does read from the area another instruction attempts to write into
- F1(a1) and G2(a2) : same as above
We suppose that instructions can read from the shared memory as long as this memory is not changed during these instructions

The goal of this application is to help user to detect non-conflicting regions of the program as these regions could be multithreaded
To do this, we use an abstraction of "dataflow"
For a given statement (n, m, k, a, (F, G, t)) we say that this statement defines flow from F(a) to G(a)
For a sequential program we say that the flow is happening from the union of F(a) to the union of G(a)
Then if two statements define flow into intesecting areas, they conflict and have to be sequential
If  program A attempts to flow into area X and program B attempts to flow away from area X, they have to be sequential
We consider a greedy strategy which could be represented via graph of flow
Vertices of this graph are elements of product (0, ..., N - 1) x Nat
Flow from collection of words A to collection of words B is represented as сomplete bipartite graph between A on layer n and B on the layer n + 1
When drawing this graph we want to shift it as close as possible to the zero along the time axis.
Then the resulting paths would showcase opportunities to do computations in parallel and points where computations will have to converge.


Storage
Word space for non-trivial programs is huge and storing all the indices would be wasteful
We store instructions as functors.
We do not care about exact t, we assume that it behave as worst as possible
Due to functoriality and all of the objects being isomorphic,
we have to store behaviour of functors only at one object: sequence of natural numbers from 1 to n
We can do it by storing the image of this sequence.

Most of functors would be "forgetful", for example:
Addition instruction takes as input sequence of three variables
Then functor F forgets about the third, and functor G forgets about the first two
Addition of variables in F and stroring the result in the G would provide us with a natural transforamation t

*/


struct
wide_word_mapping {

	size_t
	width;

	size_t
	origin;

	size_t
	destination;

};


struct
sparse_word_bijection {

	std::vector<wide_word_mapping>
	data;

	bool
	is_valid()
	{

		auto
		i
			= data.begin()->origin;

		for (

			auto
			A : data

		) {
			for (
				auto
				B :data
			) {

				if (
					A.destination < B.destination
					&& A.width + A.destination >= B.destination
				) {
					return false;
				}

				if(
					A.destination > B.destination
					&& B.width + B.destination >= A.destination
				) {
					return false;
				}

			}
		}
		return true;
	}

};


struct
generic_instruction {

	int
	n;

	std::vector<int>
	F;

	std::vector<int>
	G;

};

namespace BASIC_INSTRUCTION {

static const generic_instruction WRITE {
	2,

	// read from 0
	{
		0
	},

	// write to 1
	{
		1
	}
};

static const generic_instruction BINARY_PURE {
	// a, b -> c
	3,

	// read a b
	{0, 1},

	// write to c
	{2}
};

/*

We do not care about internal workings of pure unary operations, they are the same as copying data as far as we are concerned

*/

static const generic_instruction UNARY_PURE = WRITE;

static const generic_instruction UNARY_MUTATE {
	// a -> a, b
	2,

	// read a
	{0, },

	// write to a and b
	{0, 1}
};

static const generic_instruction BINARY_MUTATE_LEFT {
	// a, b -> a, c
	3,

	// read a b
	{0, 1},

	// write to c
	{0, 2}
};


}



struct
functorial_memory_path {
	std::vector<std::string> path {};
};

struct
resolved_memory_location {
	size_t location = 0;
};

struct
functorial_statement {

	std::vector<functorial_memory_path>
	paths_to_inputs {
	};

	generic_instruction
	applied{
	};

	bool
	await_function_to_call
	= false;

	std::string
	function_call
	= "";

	bool
	indirect_function_call
	= false;

};


struct program {

	std::vector<functorial_statement>
	sequence;

};


/*

Now we have to define interactions between types and data

For simplicity we work with undefined word space of size lower than maximal value of  C++ type "size_t"
which is larger than the configuration space of the largest runtime array
in the analyzed program it can construct while running on consumer grade PC for a few centuries.
We assume that size_t has an embedding into word space. (We want to be able to store addresses in memory)
We assume that the program is terminated before it exceeds the limitations above
We assume that primitive types occupy exactly one word.
We assume that product types occupy sequential area of memory without padding.
We assume that pointers occupy one word.
We assume that actual data of all arrays (both dynamic and static) occupy exactly one byte, no matter the actual value
We assume that access to arrays always attempts to retrieve or rewrite the said byte and doesn't attempt to access invalid indices.

References share the memory location and type with original variable.

For pointers we have to store at least some "memory content". As we assumed that size_t could be embedded into word space,
for every variable we will store a size_t value which is undefined for non-pointer values and equal to their memory address otherwise.

Conditions introduce "branches" into otherwise linear sequence.
So far I am not sure how to treat them.
For/while loops assume that only one iteration is done.

In this model complex types are just a convenient tool to navigate memory layout.

Functions assume that parameters are reserved sequentially.
Then, the fact that all K-words of the same size are isomorhpic,
allows us to extend the  program to entire functor and treat it similarly to instruction
while adjusting actual layout with required isomorhism

Function calls are resolved during post-processing stage
Value parameters are "copied" while reference parameters are left in place.
Then the tool computes "bijection" for memory layout which would conjugate applied instructions during analysis.
This bijection might be defined in a sparse way thanks to all types having a fixed size.

*/


/*

We treat types in a usual way via framework of category theory.

Product types are usually defined as a collection of projections.
But we want ot enrich them with additional data: memory offsets relatively to the variable memory location

*/


/*
Represents an object in the category of types
*/
struct
type_object {

	size_t
	id;

	bool
	is_empty() {
		return id == 0;
	}

	bool
	is_primitive()
	{
		return id == 1;
	}

};


/*

Type arrow is a morphims in category of types
For example a function int -> int is a typeflow from int to int
Consider a structure like
struct big_angry_struct { float little_happy_float; };
Then little_happy_float is a typeflow from big_angry_struct to little_happy_float

*/
struct
type_arrow {

	int
	id;

};

/*

Represents endofunctors in the category of types.

*/

struct
type_functor {
	int
	id;
};

/*

Represents morphims in the category of endofunctors in the category of types.

*/

struct
type_natural_transformation {
	int
	id;
};


/*

We treat variables as named projections of current memory layout
We also store their content if it intersects with memory addresses.

But some variables are "dirty" and intersect the imaginary boundary between instructions and data.
We store the index of the function inside them.

*/


struct
functor_id {
	int data;
};

struct
resolved_functor_id {
	int data;
};

struct
functorial_memory_layout {

	std::string
	name
	{};

	std::string
	display
	{};

	std::map<std::string, functor_id>
	projections
	{};

	functor_id
	pointer_value
	{};

	functor_id
	reference_to
	{};

	std::optional<functor_id>
	raw_type
	= std::nullopt;

	std::vector<functor_id>
	params
	{};

	std::vector<functor_id>
	type_params
	{};

	std::vector<functor_id>
	specialisations
	{};

	std::vector<functor_id>
	resolved_type_params
	{};

	functor_id
	this_reference
	{};

	std::optional<functor_id>
	function_body
	{};

	program
	local_instructions
	{};

	bool
	is_function
	= false;

	bool
	is_reference
	= false;

	bool
	is_type
	= false;

	bool
	is_param
	= false;

	bool
	is_projection
	= false;

};

struct
resolved_type_layout {

	std::optional<type_object>
	actual_type
	= std::nullopt;

	std::map<std::string, resolved_functor_id>
	projections
	{};

};

struct
resolved_memory_layout {

	resolved_memory_location
	actual_location
	{};

	std::map<std::string, resolved_memory_layout*>
	projections
	{};

};

/*

Data structures for the category of types

*/

type_object
EMPTY_TYPE {0};

type_object
EMPTY_TYPE_IDENTITY {0};

type_object
PRIMITIVE_TYPE {1};

type_arrow
PRIMITIVE_TYPE_IDENTITY_ARROW {1};

functor_id
ROOT_FUNCTOR
{0};

struct
category_of_types {

	type_object
	available_object_id = {2};

	type_arrow
	avaialble_arrow_id = {2};

	functor_id
	available_functor_id = {1};

	resolved_functor_id
	available_resolved_functor_id = {1};

	std::vector<type_object>
	arrow_start {
		EMPTY_TYPE,
		PRIMITIVE_TYPE
	};

	std::vector<type_object>
	arrow_end {
		EMPTY_TYPE,
		PRIMITIVE_TYPE
	};

	std::map<
		std::string,
		type_object
	>
	name_to_type;

	std::map<
		std::string,
		type_arrow
	>
	name_to_arrow;

	std::vector<functorial_memory_layout>
	functors
	{{}};

	std::vector<resolved_type_layout>
	resolved_functors_values
	{{}};
};


std::string
get_root_usr(
	CXCursor
	c
) {

	auto original_type = clang_getSpecializedCursorTemplate(clang_getCursorReferenced(c));

	/*

	auto
	t
	= clang_getCursorType(c);

	auto
	tc
	= clang_getTypeDeclaration(t);

	auto
	original_functor
	= tc;

	while (
		!clang_Cursor_isNull(tc)
	) {
		original_functor = tc;
		tc = clang_getSpecializedCursorTemplate(tc);
	}
	*/

	CXString
	usr_clang
	= clang_getCursorUSR (
		clang_getCursorReferenced(original_type)
	);

	std::string
	usr_cpp {
		clang_getCString(usr_clang)
	};

	clang_disposeString(usr_clang);

	return usr_cpp;
}


std::optional<type_object>
get_type (
	category_of_types&
	C,

	CXCursor
	c
)
{
	auto
	usr
	= get_root_usr(c);

	auto
	it
	= C.name_to_type.find(usr);

	if (it == C.name_to_type.end()) {
		return std::nullopt;
	} else {
		return it->second;
	}
}

type_object
request_object (
	category_of_types&
	C
)
{
	auto
	result
	= C.available_object_id;

	C.available_object_id.id++;
	return result;
}


void
provide_name(

	category_of_types&
	C,

	type_object
	A,

	std::string
	S

)
{
	C.name_to_type[S] = A;
}


void
provide_name(

	category_of_types&
	C,

	type_arrow
	A,

	std::string
	S

) {
	C.name_to_arrow[S] = A;
}


type_arrow
request_arrow (

	category_of_types&
	C,

	type_object
	A,

	type_object
	B

) {
	auto
	result
		= C.avaialble_arrow_id;

	C.avaialble_arrow_id.id ++;
	C.arrow_start.push_back(A);
	C.arrow_end.push_back(B);

	return result;
}


struct resolved_memory_stack {
	std::map<size_t, resolved_memory_location> pointer_values;
};


functor_id
allocate_space(

	category_of_types&
	C,

	functor_id
	abstract_memory,

	resolved_functor_id
	resolved_types,

	std::string
	name,

	std::string
	display

)
{
	assert(
		!name.empty()
		&& !display.empty()
	);

	C.functors.push_back({});
	C.resolved_functors_values.push_back({});
	C.functors.back().display = display;
	C.functors.back().name = name;

	auto result = C.available_functor_id;

	C.functors[abstract_memory.data].projections[name] = C.available_functor_id;
	C.available_functor_id.data++;

	C.resolved_functors_values[resolved_types.data].projections[name] = C.available_resolved_functor_id;
	C.available_resolved_functor_id.data++;

	return  result;
}

functor_id
allocate_space(

	category_of_types&
	C,

	functor_id
	abstract_memory,

	resolved_functor_id
	resolved_types,

	type_object
	actual_type,

	std::string
	name,

	std::string
	display

)
{
	assert(
		!name.empty()
		&& !display.empty()
	);

	C.functors.push_back({});
	C.resolved_functors_values.push_back({});
	C.functors.back().display = display;
	C.functors.back().name = name;
	C.resolved_functors_values.back().actual_type = actual_type;

	auto result = C.available_functor_id;

	C.functors[abstract_memory.data].projections[name] = C.available_functor_id;
	C.available_functor_id.data++;

	C.resolved_functors_values[resolved_types.data].projections[name] = C.available_resolved_functor_id;
	C.available_resolved_functor_id.data++;

	return  result;
}

functor_id
allocate_space(

	category_of_types&
	C,

	functor_id
	F,

	resolved_functor_id
	resolved_types,

	CXCursor
	c

)
{
	auto type_usr = get_root_usr(c);

		CXString
	pretty
	= clang_getCursorUSR (clang_getCursorReferenced(c));

	std::string
	usr {
		clang_getCString(pretty)
	};

	clang_disposeString(pretty);

	CXString
	display_s
	= clang_getCursorDisplayName(c);

	std::string
	display {
		clang_getCString(display_s)
	};

	clang_disposeString(display_s);


	assert(
		!usr.empty()
		&& !display.empty()
	);

	auto
	functor_exists
	=
		C
		.functors[ROOT_FUNCTOR.data]
		.projections
		.find(type_usr);

	if (
		functor_exists == C.functors[ROOT_FUNCTOR.data].projections.end()
	) {
		/*

		We do not define primitive types as functors

		*/

		return allocate_space(C, F, resolved_types, PRIMITIVE_TYPE, usr, display);
	} else {
		/*

		We already have the layout, just attach the id here.

		*/

		C.functors[F.data].projections[usr] = functor_exists->second;
		C.resolved_functors_values[resolved_types.data].projections[usr] = C.available_resolved_functor_id;
		C.available_resolved_functor_id.data++;

		return functor_exists->second;
	}
}

struct
meta_information {

	category_of_types
	types {
	};

	std::vector<functor_id>
	parser_stack
	{};

	/*

	std::vector<size_t>
	stack_size_stack
	{};

	*/

};

CXChildVisitResult
handle_generic_cursor(

	CXCursor
	cursor,

	meta_information *
	meta

) ;

bool
any_child_exist(
	CXCursor
	cursor
)
{
	bool
	exist
	= false;

	clang_visitChildren(
		cursor,
		[](
			CXCursor current_cursor,
			CXCursor parent,
			CXClientData client_data
		){
			*(bool*)client_data = true;
			return CXChildVisit_Continue;
		},
		&exist
	);

	return exist;
}


bool
child_exist(
	CXCursor
	cursor
)
{
	bool
	exist
	= false;

	clang_visitChildren(
		cursor,
		[](
			CXCursor current_cursor,
			CXCursor parent,
			CXClientData client_data
		){

			CXCursorKind
			cursor_kind
			= clang_getCursorKind(current_cursor);

			if(
				cursor_kind == CXCursor_InitListExpr
			) {
				if (child_exist(current_cursor)) {
					*(bool*)client_data = true;
				}
				return CXChildVisit_Continue;
			}

			if (
				/*
				cursor_kind == CXCursor_DeclRefExpr
				|| cursor_kind == CXCursor_CXXNullPtrLiteralExpr
				|| cursor_kind == CXCursor_IntegerLiteral
				|| cursor_kind == CXCursor_FloatingLiteral
				|| cursor_kind == CXCursor_StringLiteral
				|| cursor_kind == CXCursor_ObjCBoolLiteralExpr
				|| cursor_kind == CXCursor_CharacterLiteral
				|| cursor_kind == CXCursor_UnexposedExpr
				*/
				cursor_kind != CXCursor_TypeRef
				&& cursor_kind != CXCursor_TemplateRef
			) {
				*(bool*)client_data = true;
			}
			return CXChildVisit_Continue;
		},
		&exist
	);

	return exist;
}

void
handle_generic_cursor_children(

	CXCursor
	cursor,

	meta_information*
	meta

)
{
	clang_visitChildren(
		cursor,
		[](
			CXCursor current_cursor,
			CXCursor parent,
			CXClientData client_data
		){
			handle_generic_cursor(current_cursor, (meta_information*) client_data);
			return CXChildVisit_Continue;
		},
		meta
	);
}


CXChildVisitResult
handle_generic_cursor(

	CXCursor
	cursor,

	meta_information*
	meta

) {

	// pretty_print(cursor, "\t");

	CXCursorKind
	cursor_kind
	= clang_getCursorKind(cursor);

	CXType
	cursor_type
	= clang_getCursorType(cursor);

	CXString
	pretty
	= clang_getCursorUSR (clang_getCursorReferenced(cursor));

	std::string
	usr {
		clang_getCString(pretty)
	};




	clang_disposeString(pretty);

	CXString
	display_s
	= clang_getCursorDisplayName(cursor);

	std::string
	display {
		clang_getCString(display_s)
	};

	clang_disposeString(display_s);

	auto
	loc
	= clang_getCursorLocation(cursor);

	auto
	hash
	= clang_hashCursor(cursor);

	if (display.find("partial_ordering") != std::string::npos) {
		bool something_interesting = true;
	}

	if (display.find("_Variant_raw_get") != std::string::npos) {
		/*

		Because stack overflow

		*/
		return CXChildVisit_Break;
	}


	/*

	New parsing logic:
	Do not rely on parents.
	Accumulate parsing stack as you go.
	After parsing children, find out how many of them you need to pop.

	*/


	if (
		cursor_kind == CXCursor_StaticAssert
		|| cursor_kind == CXCursor_UnaryExpr
	) {
		return CXChildVisit_Break;
	}


	if  (
		cursor_kind == CXCursor_ParmDecl
	) {

		/*

		Children contain the type information

		*/

		auto size_before = meta->parser_stack.size();

		handle_generic_cursor_children(cursor, meta);

		/*

		The stack should be expanded by one: with calculated type

		*/

		assert(
			meta->parser_stack.size() == size_before + 1
		);

		/*

		Allocate parameter of the given type when we are back in the function.
		For now we only need a dummy in the stack

		*/

		meta->types.functors.push_back({});
		meta->types.available_functor_id.data++;
		meta->types.functors.back().name = usr;
		meta->types.functors.back().display = display;
		meta->types.functors.back().raw_type = meta->parser_stack.back();
		if (
			cursor_type.kind == CXType_LValueReference
		) {
			meta->types.functors.back().is_reference = true;
		}

		/*

		Type was consumed, push the param into the stack

		*/

		meta->parser_stack.pop_back();
		meta->parser_stack.push_back(meta->types.available_functor_id);

		return CXChildVisit_Break;
	}


	if (
		cursor_kind == CXCursor_LambdaExpr
	) {

		/*

		Lambda expression consists of:
		- Capture list (essentially fixed parameters)
		- Parameters
		- Function body

		We parse all the children into the stack

		*/

		auto size_before = meta->parser_stack.size();

		handle_generic_cursor_children(cursor, meta);

		/*

		Now the stack should contain three sections.
		Create new memory layout and fill it

		*/

		functor_id
		lambda
		= meta->types.available_functor_id;
		meta->types.available_functor_id.data++;
		meta->types.functors.push_back({});

		meta->types.functors.back().name = "Lambda" + std::to_string(clang_hashCursor(cursor));
		meta->types.functors.back().display = "Lambda";
		meta->types.functors.back().is_function = true;

		int
		stage
		= 0;

		int
		current_position
		= size_before;

		for (
			;
			current_position < meta->parser_stack.size();
			current_position++
		) {
			auto
			F_id
			= meta->parser_stack[current_position];

			auto&
			F
			= meta->types.functors[F_id.data];

			meta->types.functors.back().projections[F.name] = F_id;
			if (F.is_param) {
				meta->types.functors.back().params.push_back(F_id);
			}

			if (F.local_instructions.sequence.size() != 0) {
				meta->types.functors.back().function_body = F_id;
			}
		}

		/*

		Pop used data and push new shiny lambda to the stack

		*/

		while (meta->parser_stack.size() > size_before) {
			meta->parser_stack.pop_back();
		}
		meta->parser_stack.push_back(lambda);

		return CXChildVisit_Break;
	}


	if (cursor_kind == CXCursor_UsingDeclaration) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_UsingDirective) {
		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_FunctionDecl
		|| cursor_kind == CXCursor_FunctionTemplate
	) {

		/*
		if(usr.find("make_unique_for_overwrite") != std::string::npos) {
			return CXChildVisit_Break;
		}

		if (usr.find("_Hash_representation") != std::string::npos) {
			bool something_is_happening = true;
		}

		Function declaration consists of:
		- Template list
		- Parameters
		- Function body

		*/


		/*

		Parse all children

		*/

		auto size_before = meta->parser_stack.size();

		handle_generic_cursor_children(cursor, meta);

		functor_id
		new_function
		= meta->types.available_functor_id;
		meta->types.available_functor_id.data++;
		meta->types.functors.push_back({});

		meta->types.functors.back().name = usr;
		meta->types.functors.back().display = display;
		meta->types.functors.back().is_function = true;

		int
		stage
		= 0;

		int
		current_position
		= size_before;

		for (
			;
			current_position < meta->parser_stack.size();
			current_position++
		) {
			auto
			F_id
			= meta->parser_stack[current_position];

			auto&
			F
			= meta->types.functors[F_id.data];

			meta->types.functors.back().projections[F.name] = F_id;
			if (F.is_param) {
				if (F.is_type) {
					meta->types.functors.back().type_params.push_back(F_id);
				} else {
					meta->types.functors.back().params.push_back(F_id);
				}
			}

			if (F.local_instructions.sequence.size() != 0) {
				meta->types.functors.back().function_body = F_id;
			}
		}

		/*

		Pop used data and store the function in root

		*/

		while (meta->parser_stack.size() > size_before) {
			meta->parser_stack.pop_back();
		}

		auto
		already_exists
		= meta->types.functors[ROOT_FUNCTOR.data].projections.find(usr);

		if (already_exists == meta->types.functors[ROOT_FUNCTOR.data].projections.end()) {
			meta->types.functors[ROOT_FUNCTOR.data].projections[usr] = new_function;
		} else {
			/*

			The only new information we could retrieve is the definition of the function

			*/

			if (
				meta->types.functors[already_exists->second.data].function_body
			) {
				meta->types.functors[already_exists->second.data].function_body
				= meta->types.functors.back().function_body;
			}
		}

		/*

		We do not push function declarations to the stack

		*/

		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_FieldDecl
	) {
		/*

		In some cases the projection codomain is discovered among the children of the AST node.
		So we handle them first before attempting to resolve the type.

		*/

		auto size_before = meta->parser_stack.size();

		handle_generic_cursor_children(cursor, meta);

		auto
		field_declaration
		= meta->types.available_functor_id;

		meta->types.functors.push_back({});

		meta->types.functors.back().display = display;
		meta->types.functors.back().name = usr;
		meta->types.functors.back().is_projection = true;

		if (meta->parser_stack.size() > size_before) {
			/*

			We have detected a children type of this field.
			Make sure that this type was resolved to a single type

			*/

			assert(meta->parser_stack.size() == size_before + 1);

			auto&
			F
			= meta->types.functors[meta->parser_stack.back().data];

			assert(F.is_type);

			meta->types.functors.back().raw_type = meta->parser_stack.back();

			meta->parser_stack.pop_back();
		}

		meta->parser_stack.push_back(field_declaration);

		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_CXXMethod
	) {
		/*

		Methods are basically projections to functions
		We treat them as by adding the original type to the signature function
		Usually this parameter is a reference/pointer called `this`

		*/

		auto size_before = meta->parser_stack.size();

		handle_generic_cursor_children(cursor, meta);

		/*

		Always add reference to *this*

		*/

		functor_id
		this_reference
		= meta->types.available_functor_id;
		meta->types.available_functor_id.data++;
		meta->types.functors.push_back({});
		meta->types.functors.back().name = "This";
		meta->types.functors.back().display = "This";
		meta->types.functors.back().is_reference = true;

		functor_id
		new_method
		= meta->types.available_functor_id;
		meta->types.available_functor_id.data++;
		meta->types.functors.push_back({});

		meta->types.functors.back().name = usr;
		meta->types.functors.back().display = display;
		meta->types.functors.back().is_function = true;
		meta->types.functors.back().is_projection = true;
		meta->types.functors.back().this_reference = this_reference;

		int
		stage
		= 0;

		int
		current_position
		= size_before;

		for (
			;
			current_position < meta->parser_stack.size();
			current_position++
		) {
			auto
			F_id
			= meta->parser_stack[current_position];

			auto&
			F
			= meta->types.functors[F_id.data];

			meta->types.functors.back().projections[F.name] = F_id;
			if (F.is_param) {
				if (F.is_type) {
					meta->types.functors.back().type_params.push_back(F_id);
				} else {
					meta->types.functors.back().params.push_back(F_id);
				}
			}

			if (F.local_instructions.sequence.size() != 0) {
				meta->types.functors.back().function_body = F_id;
			}
		}

		/*

		Pop used data and store the method both in root and the stack to register as a field

		*/

		while (meta->parser_stack.size() > size_before) {
			meta->parser_stack.pop_back();
		}

		auto
		already_exists
		= meta->types.functors[ROOT_FUNCTOR.data].projections.find(usr);

		if (already_exists == meta->types.functors[ROOT_FUNCTOR.data].projections.end()) {
			meta->types.functors[ROOT_FUNCTOR.data].projections[usr] = new_method;
		} else {
			/*

			The only new information we could retrieve is the definition of the function

			*/

			if (
				meta->types.functors[already_exists->second.data].function_body
			) {
				meta->types.functors[already_exists->second.data].function_body
				= meta->types.functors.back().function_body;
			}
		}

		meta->parser_stack.push_back(new_method);

		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_TypeRef
	) {
		/*

		So we detected a type.
		Check if it already exists and create it otherwise.
		Add to stack so the parent could consume it

		*/

		auto canonical = clang_getCanonicalType(cursor_type);

		std::string canonical_name;

		{
			CXString
			pretty
			= clang_getTypeSpelling (canonical);

			canonical_name = clang_getCString(pretty);

			clang_disposeString(pretty);
		}

		assert(
			!canonical_name.empty()
		);

		auto it = meta->types.functors[ROOT_FUNCTOR.data].projections.find(canonical_name);


		if (
			it == meta->types.functors[ROOT_FUNCTOR.data].projections.end()
		) {
			auto
			new_type
			= meta->types.available_functor_id;

			meta->types.available_functor_id.data++;
			meta->types.functors.push_back({});

			meta->types.functors.back().display = canonical_name;
			meta->types.functors.back().name = canonical_name;
			meta->types.functors.back().is_type = true;

			meta->types.functors[ROOT_FUNCTOR.data].projections[canonical_name] = new_type;

			meta->parser_stack.push_back(new_type);
		} else {
			meta->parser_stack.push_back(it->second);
		}


		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_ConceptSpecializationExpr
	) {
		/*

		I am really unwilling to write a C++ compiler to process concepts...

		*/

		auto
		if_i_ignore_it_it_will_go_away
		= meta->types.available_functor_id;

		meta->types.available_functor_id.data++;
		meta->types.functors.push_back({});

		meta->types.functors.back().display = "Concept";
		meta->types.functors.back().name = "Concept";

		meta->parser_stack.push_back(if_i_ignore_it_it_will_go_away);

		return CXChildVisit_Break;
	}


	if (
		cursor_kind == CXCursor_DeclRefExpr
		|| cursor_kind == CXCursor_OverloadedDeclRef
	) {
		std::string old_usr = usr;

		auto
		original_template
		= clang_getSpecializedCursorTemplate(clang_getCursorReferenced(cursor));


		/*

		if (usr.find("_Hash_representation") != std::string::npos) {
			bool something_is_happening = true;
		}

		CXCursor_DeclRefExpr means that a certain ALREADY declared variable is referenced.
		New variables are handled with CXCursor_VarDecl
		These nodes are supposed to be AST leaves, so we do not handle children here.
		The main difficulty is usually to produce a connection
		between reference to variable and the last change to the variable change.

		But this time we only need to recover the variable itself as we emulate "memory" and can
		notice dependency via overlapping memory writes/reads.

		We still create new variables for convenience when working with instructions and function calls, but
		these variables will share memory with the original ones.

		It greatly simplifies the logic relatively to test4.cpp


		There are still multiple cases.

		0) Overload
			Unresolved templated functions/methods
			(Has children with actual information)

		1) Elaborated:
			It's a certain struct/class

		2) Dependent:
			Resolved templated structs/classes
			(Has children with actual information)

		3) Pointer
			Pointer

		4) Auto
			Compiler has to deduce the type on its own

		5) Unexposed
			Most likely a template argument

		6) FunctionProto
			Reference to a normal function.

		7)

		Sadly we have to treat all of these cases one by one.

		*/


		if (cursor_type.kind == CXType_Overload) {
			/*

			Here we try to obtain the entiry related to the function in question
			The actual logic happens in OverloadedDeclRef handler

			*/

			handle_generic_cursor_children(cursor, meta);
			return CXChildVisit_Break;
		}

		if (cursor_type.kind == CXType_Dependent) {
			/*

			In this case the code has input types provided to the template functor
			Here we have to gather them together and produce an entity with resolved type

			*/

			auto old_size = meta->parser_stack.size();
			handle_generic_cursor_children(cursor, meta);

			/*

			Make sure that there are at least 2 new elements in the stack: template and type input

			*/

			assert(
				meta->parser_stack.size() > old_size + 1
			);

			/*

			First item is supposed to be a template.
			Next items are the arguments.

			*/

			auto
			template_entity_id
			= meta->parser_stack[old_size];

			auto&
			template_entity
			= meta->types.functors[template_entity_id.data];


			/*

			Make sure that the template can be resolved with given amount of arguments

			*/

			assert(
				template_entity.type_params.size() == meta->parser_stack.size() - old_size - 1
			);

			meta->types.functors.push_back(template_entity);
			for (
				int i = old_size + 1;
				i < meta->parser_stack.size();
				i++
			) {
				meta->types.functors.back().resolved_type_params.push_back(
					meta->parser_stack[i]
				);
			}

			auto
			resolved_template
			= meta->types.available_functor_id;

			template_entity.specialisations.push_back(resolved_template);
			meta->types.available_functor_id.data++;

			while (
				meta->parser_stack.size() > old_size
			) {
				meta->parser_stack.pop_back();
			}

			meta->parser_stack.push_back(resolved_template);

			return CXChildVisit_Break;
		}

		if (cursor_type.kind == CXType_Pointer) {
			/*

			Nothing interesting, simply allocate a new functor with provided data

			*/

			assert(
				!usr.empty()
			);

			auto
			ptr
			= meta->types.available_functor_id;

			meta->types.available_functor_id.data++;
			meta->types.functors.push_back({});

			meta->types.functors.back().display = display;
			meta->types.functors.back().name = usr;

			meta->parser_stack.push_back(ptr);

			return CXChildVisit_Break;
		}

		if(cursor_type.kind == CXType_Elaborated) {
			/*

			Exactly the same as resolved template but simpler: we dont' have to create a new functor

			*/

			assert(
				meta->types.functors[ROOT_FUNCTOR.data].projections.contains(usr)
			);

			auto
			elaborated_type
			= meta->types.functors[ROOT_FUNCTOR.data].projections[usr];

			meta->parser_stack.push_back(elaborated_type);

			return CXChildVisit_Break;
		}


		if(
			cursor_type.kind == CXType_Auto
			|| cursor_type.kind == CXType_Unexposed
		) {
			/*

			There is nothing we can do.
			Create a dummy reference to something

			*/

			assert(
				!usr.empty()
			);

			auto
			ptr
			= meta->types.available_functor_id;

			meta->types.available_functor_id.data++;
			meta->types.functors.push_back({});

			meta->types.functors.back().display = display;
			meta->types.functors.back().name = usr;

			meta->parser_stack.push_back(ptr);

			return CXChildVisit_Break;
		}

		if(cursor_type.kind == CXType_FunctionProto) {
			/*

			Exactly the same as resolved template but simpler: we dont' have to create a new functor

			*/

			assert(
				meta->types.functors[ROOT_FUNCTOR.data].projections.contains(usr)
			);

			auto
			elaborated_type
			= meta->types.functors[ROOT_FUNCTOR.data].projections[usr];

			meta->parser_stack.push_back(elaborated_type);

			return CXChildVisit_Break;
		}


		/*

		To discover new opportunities

		*/

		assert(
			false
		);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_NamespaceRef || cursor_kind == CXCursor_DeclStmt) {
		handle_generic_cursor_children(cursor, meta);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_ConceptDecl) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_UnexposedAttr) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_TypedefDecl) {
		return CXChildVisit_Break;
	}

	if (cursor_kind ==  CXCursor_CXXBaseSpecifier || cursor_kind == CXCursor_CXXAccessSpecifier) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CStyleCastExpr) {
		handle_generic_cursor_children(cursor, meta);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXConstCastExpr) {
		handle_generic_cursor_children(cursor, meta);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXStaticCastExpr) {
		handle_generic_cursor_children(cursor, meta);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_BuiltinBitCastExpr) {
		handle_generic_cursor_children(cursor, meta);
		return CXChildVisit_Break;
	}

	if (cursor_kind ==CXCursor_CXXDynamicCastExpr) {
		handle_generic_cursor_children(cursor, meta);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXReinterpretCastExpr) {
		handle_generic_cursor_children(cursor, meta);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXFunctionalCastExpr) {
		handle_generic_cursor_children(cursor, meta);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_RequiresExpr) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_BreakStmt) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_FriendDecl) {
		handle_generic_cursor_children(cursor, meta);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXThrowExpr) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_PackExpansionExpr) {
		handle_generic_cursor_children(cursor, meta);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_ParenExpr) {
		if (cursor_type.kind == CXType_Dependent) {
			/*

			Skip for now

			*/

			auto
			result
			= meta->types.available_functor_id;

			meta->types.available_functor_id.data++;
			meta->types.functors.push_back({});

			meta->types.functors.back().display = "ParenExpr Dependent Dummy";
			meta->types.functors.back().name = "ParenExpr Dependent Dummy";

			meta->parser_stack.push_back(result);
		} else {
			handle_generic_cursor_children(cursor, meta);
		}
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_LinkageSpec) {
		handle_generic_cursor_children(cursor, meta);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_NullStmt) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_VarDecl) {

		/*

		Here we allocate new memory and declare variables.
		It's very simple due to out new model.

		1) Handle the right side and create required latent variables while resolving implicit memory location.
		2) This variable is in the very end of the variable list now and could be easily accesssed
		3) Allocate new variable
		4) Attach assignment statement which reads from the right side variable and writes to the left side

		*/

		variable
		target{
		};

		bool has_source = child_exist(cursor);

		if (has_source) {
			handle_generic_cursor_children(cursor, meta);
			auto&
			source
			= resolve_implicit_memory_location(
				meta->types,
				meta->top_function(),
				meta->top_function().variables.back()
			);


			if (cursor_type.kind != CXType_LValueReference) {
				target = allocate_space(
					meta->types,
					meta->top_function(),
					cursor
				);
			} else {
				target.memory_location = source.memory_location;
				target.has_memory_location = true;
			}

			/*

			We have to do a manual write when we store a reference
			to function or a memory address to preserve the value at given time;

			After parsing this information would be hard to recover.

			*/


			if (
				meta->top_function().memory_slice_pointer.contains(source.memory_location)
			) {
				meta->top_function().memory_slice_pointer[target.memory_location]
					= meta->top_function().memory_slice_pointer[source.memory_location];
			}

			if (
				meta->top_function().memory_slice_function_call.contains(source.memory_location)
			) {
				meta->top_function().memory_slice_function_call[target.memory_location]
					= meta->top_function().memory_slice_function_call[source.memory_location];
			}


			statement
			write {
				{
					source.memory_location,
					target.memory_location
				},
				BASIC_INSTRUCTION::WRITE
			};

			meta->top_function().execution.sequence.push_back(write);
		} else {
			target =  allocate_space(
				meta->types,
				meta->top_function(),
				cursor
			);
		}

		target.name = usr;
		target.display = display;

		meta->top_function().variables.push_back(target);
		meta->top_function().usr_to_variable[usr] = meta->top_function().variables.size() - 1;

		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_StructDecl
		|| cursor_kind == CXCursor_ClassTemplate // TODO
		|| cursor_kind == CXCursor_ClassDecl
	) {

		if (
			display.find("unique_ptr") != std::string::npos
			|| display.find("checked_array_iterator")  != std::string::npos
		) {
			return CXChildVisit_Break;
		}

		/*

		Currently we treat all of these cases the same way.
		But as tempaltes actually define functors rather than objects we would have to split them away later.

		Overall the logic is dead simple:
		Create a structure and recursively iterate over the FIELDS to figure out data layout.
		Then iterate over EVERYTHING ELSE.

		*/

		auto
		object
		= request_object(meta->types);

		enriched_product
		product {
			object
		};

		meta->types.discovered_products.push_back(product);
		provide_name(meta->types, object, usr);

		meta->type_stack.push_back(object);

		/*

		We don't want to iterate over methods because they are not aware of the structure size yet.

		*/

		clang_visitChildren(
			cursor,
			[]
			(
				CXCursor
				current_cursor,

				CXCursor
				parent,

				CXClientData
				client_data
			)
			{
				auto
				kind
				= clang_getCursorKind(current_cursor);

				if (
					kind == CXCursor_FriendDecl
				) {
					return CXChildVisit_Recurse;
				};

				if (
					kind == CXCursor_CXXMethod
					|| kind == CXCursor_FunctionTemplate
					|| kind == CXCursor_FunctionDecl
				) {
					return CXChildVisit_Continue;
				}

				handle_generic_cursor(
					current_cursor,
					(meta_information*)client_data
				);

				return CXChildVisit_Continue;
			},
			meta
		);

		/*

		Now the structure is aware of its own size and can participate in functions and methods

		*/

		clang_visitChildren(
			cursor,
			[]
			(
				CXCursor
				current_cursor,

				CXCursor
				parent,

				CXClientData
				client_data
			)
			{
				auto
				kind
				= clang_getCursorKind(current_cursor);

				if (
					kind == CXCursor_FriendDecl
				) {
					return CXChildVisit_Recurse;
				};

				if (
					kind != CXCursor_CXXMethod
					&& kind != CXCursor_FunctionTemplate
					&& kind != CXCursor_FunctionDecl
				) {
					return CXChildVisit_Continue;
				}

				handle_generic_cursor(
					current_cursor,
					(meta_information*)client_data
				);

				return CXChildVisit_Continue;
			},
			meta
		);

		meta->type_stack.pop_back();

		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_UnexposedExpr
		|| cursor_kind == CXCursor_CompoundStmt
		|| cursor_kind == CXCursor_Namespace
		|| cursor_kind == CXCursor_TranslationUnit
		|| cursor_kind == CXCursor_DefaultStmt
		|| cursor_kind == CXCursor_CaseStmt
		|| cursor_kind == CXCursor_DoStmt
		|| cursor_kind == CXCursor_WhileStmt
		|| cursor_kind == CXCursor_NamespaceRef
		|| cursor_kind == CXCursor_NamespaceAlias
		|| cursor_kind == CXCursor_CXXTryStmt
		|| cursor_kind == CXCursor_CXXCatchStmt
	) {
		handle_generic_cursor_children(cursor, meta);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_ConversionFunction) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_UnexposedStmt) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXDeleteExpr) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_TemplateRef) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_TemplateTemplateParameter) {
		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_CXXThisExpr
	) {
		/*

		We assume that "this" is always at memory location 0

		*/

		variable
		local_this {
		};

		local_this.assumed_type = meta->type_stack.back();
		local_this.display = "this";
		local_this.name = "this";

		auto&
		fun
		= meta->top_function();

		fun.variables.push_back(local_this);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXNewExpr) {

		/*

		We assumed that under no conditions we would ever need more than 1 word to represent any given array
		It  simplifies the logic of `new` expression heavily

		*/

		auto&
		fun
		= meta->top_function();

		auto
		variable
		= allocate_space(meta->types, fun, PRIMITIVE_TYPE);

		fun.variables.push_back(variable);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_InitListExpr) {

		/*

		TODO
		Init list is essentially a sequence of copy operations which fill the structure fields one by one


		point new_value {};
		new_value.name = "INIT";
		new_value.hash = hash;
		new_value.display_name = "init list";
		new_value.is_init_list = true;
		new_value.location = loc;

		current_scope->call_stack.push_back(new_value);

		clang_visitChildren(
			cursor,
			[](
				CXCursor current_cursor,
				CXCursor parent,
				CXClientData client_data
			){
				auto scope = (code*) client_data;
				handle_generic_cursor(current_cursor, (code*)client_data);
				auto& call = scope->call_stack.back();
				call.computation.push_back(scope->points.size() - 1);
				return CXChildVisit_Continue;
			},
			current_scope
		);

		auto updated = current_scope->call_stack.back();
		current_scope->call_stack.pop_back();

		auto scope = current_scope->flow_stack.back();
		current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
		current_scope->points.push_back(updated);


		*/

		handle_generic_cursor_children(cursor, meta);

		return CXChildVisit_Break;
	}

	/*

	Here comes a sequence of literal expressions.
	We allocate memory to store them on demand to avoid excess dependencies and complex memory management

	*/

	if (
		cursor_kind == CXCursor_CXXNullPtrLiteralExpr
	) {
		auto
		variable
		= allocate_space(
			meta->types,
			meta->top_function(),
			cursor
		);

		variable.display = "Null";
		variable.name = "null";

		meta->top_function()
			.variables
			.push_back(variable);


		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_SizeOfPackExpr
	) {
		auto
		variable
		= allocate_space(
			meta->types,
			meta->top_function(),
			cursor
		);

		variable.display = "Size of";
		variable.name = "size_of";

		meta->top_function()
			.variables
			.push_back(variable);


		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_VariableRef) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_ContinueStmt) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXTypeidExpr) {
		return CXChildVisit_Break;
	}

	if(cursor_kind == CXCursor_CXXBoolLiteralExpr) {
		auto data = clang_Cursor_Evaluate(cursor);
		auto data_int = clang_EvalResult_getAsInt(data);

		auto
		variable
		= allocate_space(
			meta->types,
			meta->top_function(),
			cursor
		);

		if (data_int) {
			variable.name = "LITERALLY_TRUE";
			variable.display = "true";
		} else {
			variable.name = "LITERALLY_FALSE";
			variable.display = "false";
		}

		meta->top_function()
			.variables
			.push_back(variable);


		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_IntegerLiteral) {
		auto
		variable
		= allocate_space(
			meta->types,
			meta->top_function(),
			cursor
		);

		auto data = clang_Cursor_Evaluate(cursor);
		auto data_int = clang_EvalResult_getAsInt(data);

		variable.name = "LITERALLY_" + std::to_string(data_int);
		variable.display = std::to_string(data_int);

		meta->top_function()
			.variables
			.push_back(variable);


		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_FloatingLiteral) {
		auto
		variable
		= allocate_space(
			meta->types,
			meta->top_function(),
			cursor
		);

		auto data = clang_Cursor_Evaluate(cursor);
		auto data_int = clang_EvalResult_getAsDouble(data);

		variable.name = "LITERALLY_" + std::to_string(data_int);
		variable.display = std::to_string(data_int);

		meta->top_function()
			.variables
			.push_back(variable);


		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_StringLiteral
	) {
		auto
		variable
		= allocate_space(
			meta->types,
			meta->top_function(),
			cursor
		);

		variable.name = "LITERALLY_STRING";
		variable.display = "String";

		meta->top_function()
			.variables
			.push_back(variable);


		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_CharacterLiteral
	) {
		auto
		variable
		= allocate_space(
			meta->types,
			meta->top_function(),
			cursor
		);

		variable.name = "LITERALLY_CHAR";
		variable.display = "Character";

		meta->top_function()
			.variables
			.push_back(variable);


		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_TemplateTypeParameter) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_ClassTemplatePartialSpecialization) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_NonTypeTemplateParameter) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_TypeAliasDecl) {
		return CXChildVisit_Break;
	}
	if (cursor_kind == CXCursor_TypeAliasTemplateDecl) {
		return CXChildVisit_Break;
	}

	if (
		clang_isAttribute(cursor_kind)
	) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_Constructor || cursor_kind == CXCursor_Destructor) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_EnumDecl || cursor_kind == CXCursor_EnumConstantDecl) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_MemberRef) {
		return CXChildVisit_Break;
	}


	if (
		cursor_kind == CXCursor_CallExpr
	) {

		if (cursor_type.kind == CXType_Dependent) {
			// return CXChildVisit_Break;
		}

		std::string old_usr = usr;

		auto
		original_template
		= clang_getSpecializedCursorTemplate(clang_getCursorReferenced(cursor));

		if (
			!clang_Cursor_isNull(original_template)
		) {
			CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(original_template));
			usr = clang_getCString(pretty);
			clang_disposeString(pretty);
		}

		auto
		referenced
		= clang_getCursorReferenced(cursor);

		if (!clang_Cursor_isNull(referenced)) {
			auto ref_kind = clang_getCursorKind(referenced);
			if (ref_kind == CXCursor_Constructor) {
				// handle_generic_cursor_children(cursor, meta);
				// return CXChildVisit_Break;
			}
		}

		statement
		command {
		};

		/*

		In some cases USR is not provided here and this assignment does nothing.
		We have to look into callable children and obtain it.
		More detailed decription could be found in the MemberRefExpr handler

		*/

		command.function_call = usr;

		if (usr.size() == 0) {
			command.await_function_to_call = true;
		}

		meta->call_stack.push_back(command);
		clang_visitChildren(
			cursor,
			[](
				CXCursor current_cursor,
				CXCursor parent,
				CXClientData client_data
			){
				auto
				scope
				= (meta_information*) client_data;

				auto cursor_kind = clang_getCursorKind(current_cursor);

				if (
					cursor_kind == CXCursor_TypeRef
					|| cursor_kind == CXCursor_TemplateRef
				) {
					return CXChildVisit_Continue;
				}

				handle_generic_cursor(
					current_cursor,
					scope
				);

				auto&
				current_scope
				= scope->top_function();

				auto&
				call
				= scope->call_stack.back();

				auto&
				arg
				= current_scope.variables.back();

				auto
				new_size
				= type_size(
					scope->types,
					arg.assumed_type
				);

				call.input.push_back(arg.memory_location);

				return CXChildVisit_Continue;
			},
			meta
		);

		auto &
		updated_command
		= meta->call_stack.back();

		/*

		Sometimes libclang is stuck and doesn't parse certain function calls properly
		It is what it is: we skip these calls by treating them as a custom command

		*/

		if (updated_command.function_call.size() == 0) {
			updated_command.function_call = "";
			updated_command.await_function_to_call = false;
			updated_command.indirect_function_call = false;
			for (
				auto i = 0;
				i < updated_command.input.size();
				i++
			) {
				updated_command.applied.F.push_back(i);
			}
			variable
			dummy
			= allocate_space(meta->types, meta->top_function(), PRIMITIVE_TYPE);
			dummy.name = "Unknown call";
			updated_command.input.push_back(dummy.memory_location);

			updated_command.applied.G.push_back(updated_command.input.size() - 1);

			meta->top_function().execution.sequence.push_back(updated_command);
			meta->call_stack.pop_back();
			meta->top_function().variables.push_back(dummy);
		} else {
			variable
			dummy
			= allocate_space(meta->types, meta->top_function(), PRIMITIVE_TYPE);
			dummy.name = "return_" + updated_command.function_call;
			dummy.display = "Return value " + display;
			dummy.is_return_dummy = true;

			updated_command.applied.G.push_back(dummy.memory_location);

			meta->top_function().execution.sequence.push_back(updated_command);
			meta->call_stack.pop_back();
			meta->top_function().variables.push_back(dummy);
		}


		// updated_command.input.erase(updated_command.input.begin());


		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_ArraySubscriptExpr) {

		/*

		Subscripting is a complex operation.
		It's the main source of implicit memory locations.

		But we don't care about the exact index, the only things which matter are:

		1) Part of memory we index.
		2) Part of memory we index with.

		First part is represented with `memory_location` field of a variable.
		The second one is stored in `impllicit_memory_location` field.
		The precise relation "this word is used to index that segement of memory" doesn't matter.

		*/

		auto
		original_template
		= clang_getSpecializedCursorTemplate(clang_getCursorReferenced(cursor));

		if (!clang_Cursor_isNull(original_template)) {
			CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(original_template));
			usr = clang_getCString(pretty);
			clang_disposeString(pretty);
		}

		variable
		accessed_memory {
		};

		accessed_memory.name = "Subscript";
		accessed_memory.display = "Subscript";

		/*

		Add index to implicit memory location

		*/

		meta->variable_stack.push_back(accessed_memory);

		clang_visitChildren(
			cursor,
			[](
				CXCursor current_cursor,
				CXCursor parent,
				CXClientData client_data
			){
				auto
				scope
				= (meta_information*) client_data;

				auto
				current_type
				= clang_getCursorType(current_cursor);

				if (
					current_type.kind == CXType_Pointer
					|| current_type.kind == CXType_ConstantArray
				)  {
					return CXChildVisit_Continue;
				}

				handle_generic_cursor(
					current_cursor,
					scope
				);

				auto&
				subscript
				= scope->variable_stack.back();

				auto&
				index
				= scope->top_function().variables.back();

				subscript.implicit_memory_location.push_back(index.memory_location);
				for (
					auto i : index.implicit_memory_location
				) {
					subscript.implicit_memory_location.push_back(i);
				}

				return CXChildVisit_Continue;
			},
			meta
		);

		/*

		handle what we actually access

		*/

		clang_visitChildren(
			cursor,
			[](
				CXCursor current_cursor,
				CXCursor parent,
				CXClientData client_data
			){

				auto
				scope
				= (meta_information*) client_data;

				auto
				current_type
				= clang_getCursorType(current_cursor);

				if (
					current_type.kind != CXType_Pointer
					&& current_type.kind != CXType_ConstantArray
					&& current_type.kind != CXType_Auto
				) {
					return CXChildVisit_Continue;
				}

				handle_generic_cursor(
					current_cursor,
					(meta_information*)client_data)
				;

				auto&
				subscript
				= scope->variable_stack.back();

				auto&
				indexed
				= scope->top_function().variables.back();

				subscript.memory_location = indexed.memory_location;
				subscript.has_memory_location = true;
				for (
					auto i : indexed.implicit_memory_location
				) {
					subscript.implicit_memory_location.push_back(i);
				}

				return CXChildVisit_Continue;
			},
			meta
		);

		/*

		Now we are ready to actually place the variable into the list

		*/

		auto
		final
		= meta->variable_stack.back();

		meta->top_function()
			.variables
			.push_back(final);

		meta->variable_stack.pop_back();

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_MemberRefExpr) {

		/*

		In this segment we handle access to fields of structure.

		Usual structure is:
		- Field A of struct a with global offset 5
		- - Field B of struct b with global offset 2
		- - - Reference to the object of type b

		Fields are type projections enriched with memory offsets, so we merely have to track and add their offsets to traverse them.

		In the example above we are starting with 0 offset.
		Then we encounter field A and add 5 to the tracked offset. Field B adds 2 to this offset.
		When we arrive at b, we know that the initial value is located at location of b shifted by 2 + 5 = 7 words.

		It would be nice to work through templates as well eventually.
		Doesn't sound too hard because libclang knows something about the types in templates but it's not the number 1 priority task.

		*/

		auto
		original_template
		= clang_getSpecializedCursorTemplate(clang_getCursorReferenced(cursor));

		if (
			!clang_Cursor_isNull(original_template)
		) {
			CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(original_template));
			usr = clang_getCString(pretty);
			clang_disposeString(pretty);
		}

		if (usr == "") {
			/*

			It's an odd case when LibClang doesn't provide member information.
			Just ignore it and handle children instead.

			*/


			if (any_child_exist(cursor)) {
				handle_generic_cursor_children(cursor, meta);
			} else {
				assert(false);
			}

			return  CXChildVisit_Break;
		}

		auto
		projection_it
		= meta->types.name_to_arrow.find(usr);


		if (
			projection_it == meta->types.name_to_arrow.end()
		) {
			/*

			Can it actually happen in valid code?
			Seems that the answer is YES.
			When something attempts to call a method,
			it could attempt to reference it as a member reference here!
			We want to handle this case inside the CallExpr when
			the function's USR is somehow omitted there.
			So we want to return some kind of a reference to the function
			which could be used inside CallExpr

			*/

			auto method = meta->usr_to_function.find(usr);

			if (method == meta->usr_to_function.end()) {
				// assert(
					// false
				// );
			} else {
				assert(
					!meta->call_stack.empty()
				);

				if (
					meta->call_stack.back().function_call.size() == 0
					&& meta->call_stack.back().await_function_to_call
				) {
					meta->call_stack.back().function_call = usr;
					meta->call_stack.back().await_function_to_call = false;
				}
			}



			return  CXChildVisit_Break;
		}

		auto offset = meta->types.arrow_memory_offset[projection_it->second.id];

		// meta->current_offset.push_back(offset);

		clang_visitChildren(
			cursor,
			[](

				CXCursor
				current_cursor,

				CXCursor
				parent,

				CXClientData
				client_data

			){

				auto
				scope
				= (meta_information*) client_data;

				handle_generic_cursor(current_cursor, scope);

				return CXChildVisit_Continue;
			},
			meta
		);

		// meta->current_offset.pop_back();

		/*

		We have obtained the object we want to project.
		Now we create a latent variable which references the offset location of the projected object.

		*/

		auto
		has_child_node
		= any_child_exist(cursor);

		int parent_index = meta->top_function().variables.size() - 1;

		if (!has_child_node) {
			variable
			implicit_this {
			};

			implicit_this.memory_location = 0;
			implicit_this.assumed_type = meta->types.arrow_start[projection_it->second.id];
			implicit_this.display = "Implicit this";
			implicit_this.name = "this";
			meta->top_function().variables.push_back(implicit_this);
			parent_index++;
		}

		auto&
		parent
		= meta
			->top_function()
			.variables[parent_index];

		variable
		field_location {
		};

		field_location.memory_location
			= parent
			.memory_location
			+
			offset;

		field_location.name = usr;
		field_location.display = display;

		/*

		We have to inherit implicit location due to things like `v[i].value`

		*/

		for (
			auto
			i : parent.implicit_memory_location
		) {
			field_location.implicit_memory_location.push_back(i);
		}

		meta->top_function().variables.push_back(field_location);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_ReturnStmt) {

		assert(
			meta->function_stack.size() > 0
		);

		handle_generic_cursor_children(cursor, meta);

		auto&
		current_function
		= meta->top_function();


		current_function.return_variable_index = current_function.variables.size() -1;


		return CXChildVisit_Break;
	}

	/*
	if (
		cursor_kind == CXCursor_UnaryOperator
		&& clang_getCursorUnaryOperatorKind(cursor) == CXUnaryOperator_Deref
	) {
		handle_generic_cursor_children(cursor, current_scope);
		// auto latest_assignment = last_mention(current_scope, std::string name_to_find)
	}
	*/

	if (
		cursor_kind == CXCursor_UnaryOperator
	) {
		if (meta->top_function().await_definition) {
			return CXChildVisit_Break;
		}

		bool
		mutates
		= false;

		auto
		binary
		= clang_getCursorUnaryOperatorKind(cursor);

		if (
			binary == CXUnaryOperator_PostDec
			|| binary == CXUnaryOperator_PostInc
			|| binary == CXUnaryOperator_PreDec
			|| binary == CXUnaryOperator_PreInc
		) {
			mutates = true;
		}

		auto&
		function
		= meta->top_function();

		handle_generic_cursor_children(cursor, meta);

		auto&
		input
		= function.variables.back();

		variable
		result
		= allocate_space(meta->types, function, cursor);

		result.display = display;
		result.name = usr;

		statement
		to_add {
		};

		to_add.input = {input.memory_location, result.memory_location};

		if (
			mutates
		) {
			to_add.applied = BASIC_INSTRUCTION::UNARY_MUTATE;
		} else {
			to_add.applied = BASIC_INSTRUCTION::UNARY_PURE;
		}

		function.variables.push_back(result);
		function.execution.sequence.push_back(to_add);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_UnionDecl) {
		return CXChildVisit_Break;
	}

	if(cursor_kind == CXCursor_UnexposedDecl) {
		auto dummy = allocate_space(meta->types, meta->top_function(), PRIMITIVE_TYPE);
		dummy.name = "Unexposed";
		dummy.display = "Unexposed";
		meta->top_function().variables.push_back(dummy);
		return CXChildVisit_Break;
	}

	if(
		cursor_kind == CXCursor_IfStmt
		|| cursor_kind == CXCursor_ForStmt
		|| cursor_kind == CXCursor_CXXForRangeStmt
		|| cursor_kind == CXCursor_SwitchStmt
		|| cursor_kind == CXCursor_ConditionalOperator
	){

		/*

		We treat flow control as normal code currently

		*/

		handle_generic_cursor_children(cursor, meta);

		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_BinaryOperator
		|| cursor_kind == CXCursor_CompoundAssignOperator
	) {
		if (meta->top_function().await_definition) {
			return CXChildVisit_Break;
		}

		bool mutates_left = false;
		bool left_is_input = true;
		auto binary = clang_getCursorBinaryOperatorKind(cursor);
		if (
			binary == CXBinaryOperator_Assign
			|| binary == CXBinaryOperator_AddAssign
			|| binary == CXBinaryOperator_MulAssign
			|| binary == CXBinaryOperator_SubAssign
			|| binary == CXBinaryOperator_DivAssign
			|| binary == CXBinaryOperator_AndAssign
			|| binary == CXBinaryOperator_OrAssign
			|| cursor_kind == CXCursor_CompoundAssignOperator
			|| cursor_kind == CXCursor_VarDecl
		) {
			mutates_left = true;
		}

		if (binary == CXBinaryOperator_Assign || cursor_kind == CXCursor_VarDecl ) {
			left_is_input = false;
		}

		statement
		to_add {
		};


		if (
			mutates_left
		) {
			to_add.applied = BASIC_INSTRUCTION::BINARY_MUTATE_LEFT;
		} else {
			to_add.applied = BASIC_INSTRUCTION::BINARY_PURE;
		}

		meta->call_stack.push_back(to_add);

		clang_visitChildren(
			cursor,
			[](

				CXCursor
				current_cursor,

				CXCursor
				parent,

				CXClientData
				client_data

			){

				auto
				scope
				= (meta_information*) client_data;

				handle_generic_cursor(current_cursor, scope);

				auto&
				command
				= scope->call_stack.back();

				command.input.push_back(
					scope
					->top_function()
					.variables
					.back()
					.memory_location
				);

				return CXChildVisit_Continue;
			},
			meta
		);

		variable
		result
		= allocate_space(
			meta->types,
			meta->top_function(),
			cursor
		);

		result.name = "BINARY";
		if (cursor_kind == CXCursor_BinaryOperator) {
			auto str = clang_getBinaryOperatorKindSpelling(binary);
			result.display = clang_getCString(str);
			clang_disposeString(str);
		} else {
			result.name = "COMPOUND_ASSIGN";
			result.display = "+=";
		}

		meta->call_stack.back().input.push_back(result.memory_location);
		meta->top_function().variables.push_back(result);

		meta->top_function().execution.sequence.push_back(meta->call_stack.back());
		meta->call_stack.pop_back();

		return CXChildVisit_Break;
	}

	// std::cout << "end: " << usr << "\n";
	pretty_print(cursor, "\t\t");
	std::cout << "UNRESOLVED CASE\n";

	handle_generic_cursor_children(cursor, meta);

	return CXChildVisit_Recurse;
}

int
map_memory(

	std::vector<std::map<int, int>>&
	memory_mapping,

	int memory_offset,

	size_t location

) {
	auto
	it
	= memory_mapping.back().find(location);
	if (it == memory_mapping.back().end()) {
		return (int) location + memory_offset;
	}

	int current_layer = memory_mapping.size() - 1;
	auto true_loc = -1;
	while (it != memory_mapping[current_layer].end() && current_layer > 0) {
		if (true_loc == it->second) {
			break;
		}
		true_loc = it->second;
		current_layer--;
		it = memory_mapping[current_layer].find(true_loc);
	}
	if (it != memory_mapping[current_layer].end() ) {
		true_loc = it->second;
	}
	return true_loc;
}

void
execute_simple_statement(

	std::map<int, std::string>&
	memory_location_name,

	int&
	prefix,

	meta_information&
	meta,

	function_description&
	flow,

	std::map<int, int>&
	memory_access_before,

	std::map<int, int>&
	memory_access_after,

	std::vector<std::map<int, int>>&
	memory_mapping,

	statement&
	item,

	int
	memory_offset

) {

	auto
	true_memory_location
	= [&] (
		size_t
		location
	) {
		return  map_memory(memory_mapping, memory_offset, location);
	};

	auto
	get_name_memory_location
	= [&] (
		size_t true_location
	) {
		auto
		it
		= memory_location_name.find(true_location);

		if (it != memory_location_name.end()) {
			return it->second;
		}

		/*
		for (
			auto item : flow.variables
		) {
			if (
				true_memory_location(item.memory_location) <= true_location
				&& true_memory_location(item.memory_location) + type_size(meta.types, item.assumed_type) > true_location
			) {
				memory_location_name[true_location] = "w" + std::to_string(true_location) + "_" + ninja_name(item.display);
				return memory_location_name[true_location];
			}
		}
		*/

		return "word_" + std::to_string(true_location);
	};

	auto&
	input
	= item.input;

	for (
		auto
		F_value
		: item.applied.F
	) {
		auto F_loc = true_memory_location(input[F_value]);
		for (
			auto
			G_value
			: item.applied.G
		) {
			auto G_loc = true_memory_location(input[G_value]);
			std::cout
				<< "q" << prefix
				<< "_" << get_name_memory_location(F_loc)
				<< "_W"
				<< memory_access_before[F_loc]
				<< "->"
				<< "q" << prefix
				<< "_" << get_name_memory_location(G_loc)
				<< "_W"
				<< memory_access_after[G_loc]
				<< "\n";
		}
	}
}

std::optional<size_t> execute_function(

	std::map<int, std::string>&
	memory_location_name,

	int&
	prefix,

	meta_information&
	meta,

	std::map<int, int>&
	memory_access,

	function_description&
	flow,

	std::map<std::string, int> &
	visit_count,

	std::map<size_t, std::string>&
	memory_slice_functions,

	std::vector<std::map<int, int>>&
	memory_mapping,

	int
	memory_offset,

	int&
	next_memory_offset

) {

	assert(
		flow.name != "Root"
	);

	next_memory_offset = memory_offset + flow.available_memory_location;

	if (!visit_count.contains(flow.name)) {
		visit_count[flow.name] = 0;
	} else {
		visit_count[flow.name]++;
		if (visit_count[flow.name] > 10) {
			next_memory_offset++;
			return next_memory_offset - 1;
		}
	}

	auto
	true_memory_location
	= [&] (
		size_t
		location
	) {
		return  map_memory(memory_mapping, memory_offset, location);
	};


	for (auto& [k, v] : flow.memory_slice_function_call) {
		memory_slice_functions[true_memory_location(k)] = v;
	}

	auto
	get_memory_write_count
	= [&] (
		size_t true_location
	) {
		auto
		it
		= memory_access.find(true_location);

		if (it == memory_access.end()) {
			memory_access[true_location] = 0;
			return 0;
		}
		return  it->second;
	};
	auto
	inc_memory_write_count
	= [&] (
		size_t true_location
	) {
		auto
		it
		= memory_access.find(true_location);

		if (it == memory_access.end()) {
			memory_access[true_location] = 0;
		}
		memory_access[true_location]++;
	};

	auto
	get_name_memory_location
	= [&] (
		size_t true_location
	) {
		auto
		it
		= memory_location_name.find(true_location);

		if (it != memory_location_name.end()) {
			return it->second;
		}

		/*
		for (
			auto item : flow.variables
		) {
			if (
				true_memory_location(item.memory_location) <= true_location
				&& true_memory_location(item.memory_location) + type_size(meta.types, item.assumed_type) > true_location
			) {
				memory_location_name[true_location] = "w" + std::to_string(true_location) + "_" + ninja_name(item.display);
				return memory_location_name[true_location];
			}
		}
		*/

		return "word_" + std::to_string(true_location);
	};



	std::map<std::string, bool> mentioned;
	for (
		auto&
		item : flow.variables
	) {
		if (
			mentioned.contains(item.name)
			|| !item.has_memory_location
		) {
			continue;
		}
		auto
		location
		= true_memory_location(item.memory_location);
		for (int shift = 0; shift < type_size(meta.types, item.assumed_type); shift++) {
			std::cout
				<< "q" << prefix  << "off" << memory_offset
				<< "_" << ninja_name(item.display)
				<< "->"
				<< "q" << prefix << "_"
				<< get_name_memory_location(location + shift)
				<< "_W"
				<< get_memory_write_count(location + shift)
				<< "\n";
		}
		mentioned[item.name] = true;
	}

	for (
		auto&
		item
		: flow.execution.sequence
	) {


		auto&
		input
		= item.input;


		if (
			item.function_call.size() > 0
		) {

			auto function_usr = item.function_call;

			auto
			it
			= meta.usr_to_function.find(function_usr);

			bool invalid_function = false;

			if (it == meta.usr_to_function.end()) {
				/*

				Try to look for the variable instead and check the memory content

				*/

				auto
				variable_index
				= flow.usr_to_variable[function_usr];

				auto&
				variable
				= flow.variables[variable_index];

				auto it2
				= memory_slice_functions
					.find(
						true_memory_location(
							variable.memory_location
						)
					);

				if (it2 != memory_slice_functions.end()) {
					function_usr = it2->second;
				} else {
					function_usr = "";
					invalid_function = true;
					// assert(false);
				}
			}

			if (invalid_function) {
				continue;;
			}

			auto&
			next_function
			= meta.functions[meta.usr_to_function[function_usr]];

			std::map<int, int>
			new_mapping {
			};

			int skip_input = 0;
			int skip_parameter = 0;

			if (next_function.is_method) {
				/*

				0 -> this

				1 -> function

				2 and so on -> parameters

				*/

				auto
				this_location
				= next_function.this_location;

				auto
				actual_location
				= true_memory_location(item.input[0]);

				auto
				size
				= type_size(meta.types, next_function.variables[0].assumed_type);

				for (
					int j = this_location;
					j < size + this_location;
					j++
				) {
					new_mapping[j] = actual_location;
					if (next_function.memory_slice_function_call.contains(j)) {
						memory_slice_functions[actual_location] = next_function.memory_slice_function_call[j];
					}
				}
				skip_parameter = 1;
				skip_input = 2;
			} else if (next_function.parameters_count < item.input.size()){
				skip_parameter = 0;
				skip_input = 1;
			}

			for(
				auto i = skip_input;
				i < item.input.size();
				i++
			) {
				auto
				param
				= next_function.variables[i - skip_input + skip_parameter];

				auto
				next_func_location
				= param.memory_location;

				auto
				actual_location
				= true_memory_location(item.input[i]);

				auto
				size
				= type_size(meta.types, param.assumed_type);

				for (
					int j = next_func_location;
					j < size + next_func_location;
					j++
				) {
					new_mapping[j] = actual_location;
					if (next_function.memory_slice_function_call.contains(j)) {
						memory_slice_functions[actual_location] = next_function.memory_slice_function_call[j];
					}
				}
			}

			memory_mapping.push_back(new_mapping);
			auto returned_location = execute_function(
				memory_location_name,
				prefix,
				meta,
				memory_access,
				next_function,
				visit_count,
				memory_slice_functions,
				memory_mapping,
				next_memory_offset,
				next_memory_offset
			);
			memory_mapping.pop_back();

			if (returned_location) {
				auto dummy_location = true_memory_location(item.applied.G[0]);
				for (auto& all_mmap : memory_mapping) {
					all_mmap[dummy_location] = returned_location.value();
				}
				if (memory_slice_functions.contains(dummy_location)) {
					memory_slice_functions[returned_location.value()] = memory_slice_functions[dummy_location];
				}
			}

		} else {
			std::map<int, int> memory_input;

			for (
				auto
				F_value
				: item.applied.F
			) {
				auto true_loc = true_memory_location(input[F_value]);
				memory_input[true_loc] = get_memory_write_count(true_loc);
			}

			for (
				auto
				G_value
				: item.applied.G
			) {
				auto true_loc = true_memory_location(input[G_value]);
				inc_memory_write_count(true_loc);
			}
			execute_simple_statement(
				memory_location_name,
				prefix,
				meta,
				flow,
				memory_input,
				memory_access,
				memory_mapping,
				item,
				memory_offset
			);
		}
	}

	{
		std::map<std::string, bool> mentioned;
		for (
			auto&
			item : flow.variables
		) {
			if (
				mentioned.contains(item.name)
				|| !item.has_memory_location
			) {
				continue;
			}
			auto
			location
			= true_memory_location(item.memory_location);
			for (int shift = 0; shift < type_size(meta.types, item.assumed_type); shift++) {
				std::cout
					<< "q" << prefix  << "off" << memory_offset
					<< "_" << ninja_name(item.display)
					<< "->"
					<< "q" << prefix << "_"
					<< get_name_memory_location(location + shift)
					<< "_W"
					<< get_memory_write_count(location + shift)
					<< "\n";
			}
			mentioned[item.name] = true;
		}
	}

	if (flow.return_variable_index >= 0) {
		return true_memory_location(flow.variables[flow.return_variable_index].memory_location);
	} else {
		return std::nullopt;
	}
}

int
main()
{
	CXIndex index = clang_createIndex(0, 0); //Create index
	CXTranslationUnit unit;

	std::vector<const char*> flags2 {"clang++", "-std=c++23", "-mavx2", "-mfma", "-DGLEW_NO_GLU","-DGLEW_STATIC","-DGLM_ENABLE_EXPERIMENTAL","-DINCREMENTAL=1","-DPROJECT_ROOT=C:/_projects/cpp/alice","-DVC_EXTRALEAN","-DWIN32_MEAN_AND_LEAN","-D_CRT_SECURE_NO_WARNINGS","-IC:/_projects/cpp/alice/src","-IC:/_projects/cpp/alice/src/ai","-IC:/_projects/cpp/alice/src/common_types","-IC:/_projects/cpp/alice/src/filesystem","-IC:/_projects/cpp/alice/src/gamestate","-IC:/_projects/cpp/alice/src/gui","-IC:/_projects/cpp/alice/src/gui/province_tiles","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/diplomacy_subwindows","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/production_subwindows","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/politics_subwindows","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/military_subwindows","-IC:/_projects/cpp/alice/src/graphics","-IC:/_projects/cpp/alice/src/parsing","-IC:/_projects/cpp/alice/src/window","-IC:/_projects/cpp/alice/src/text","-IC:/_projects/cpp/alice/src/sound","-IC:/_projects/cpp/alice/src/map","-IC:/_projects/cpp/alice/src/network","-IC:/_projects/cpp/alice/src/nations","-IC:/_projects/cpp/alice/src/gamerule","-IC:/_projects/cpp/alice/src/provinces","-IC:/_projects/cpp/alice/src/economy","-IC:/_projects/cpp/alice/src/culture","-IC:/_projects/cpp/alice/src/military","-IC:/_projects/cpp/alice/src/scripting","-IC:/_projects/cpp/alice/src/scripting/luajit","-IC:/_projects/cpp/alice/src/zstd","-IC:/_projects/cpp/alice/src/lunasvg","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/glew-src/include/GL","-IC:/_projects/cpp/alice/ankerl","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/glew-src/include","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/freetype-build/include","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/freetype-src/include","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/harfbuzz-src/src","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/glm-src","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/datacontainer-src/CommonIncludes","-IC:/_projects/cpp/alice/dependencies/stb/.","-o economy.o"
	};

	std::vector<const char*> flags {"clang++", "-std=c++23"};

	clang_parseTranslationUnit2FullArgv(
		index,
		"test_function.cpp",
		// "C:/_projects/cpp/alice/src/economy/economy.cpp",
		// "C:/_projects/cpp/alice/src/economy/economy_amalg.cpp",
		flags.data(), flags.size(),
		// flags2.data(), flags2.size(),
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

	CXCursor cursor = clang_getTranslationUnitCursor(unit); //Obtain a cursor at the root of the translation unit

	meta_information code_db{};

	function_description
	root {
	};

	root.name = "Root";

	code_db.functions.push_back(root);

	code_db.function_stack.push_back(0);

	{
		function_description
		m128_assign {
		};

		m128_assign.display = "Assign m128";
		m128_assign.available_memory_location = 0;

		variable
		first_var
		= allocate_space(code_db.types, m128_assign, PRIMITIVE_TYPE);

		variable
		second_var
		= allocate_space(code_db.types, m128_assign, PRIMITIVE_TYPE);

		m128_assign.is_method = true;
		m128_assign.parameters_count = 1;
		m128_assign.execution.sequence.push_back(
			{
				{0, 1},
				BASIC_INSTRUCTION::WRITE,
			}
		);

		m128_assign.variables.push_back(first_var);
		m128_assign.variables.push_back(second_var);

		code_db.usr_to_function["c:@U@__m128i@F@operator=#&&$@U@__m128i#"] = code_db.functions.size();
		code_db.functions.push_back(m128_assign);
	}

	{
		function_description
		OPERATOR_BINARY {
		};

		OPERATOR_BINARY.display = "operator_binary";
		OPERATOR_BINARY.available_memory_location = 0;

		variable
		first_var
		= allocate_space(code_db.types, OPERATOR_BINARY, PRIMITIVE_TYPE);

		variable
		second_var
		= allocate_space(code_db.types, OPERATOR_BINARY, PRIMITIVE_TYPE);

		variable
		result
		= allocate_space(code_db.types, OPERATOR_BINARY, PRIMITIVE_TYPE);

		OPERATOR_BINARY.is_method = true;
		OPERATOR_BINARY.parameters_count = 1;
		OPERATOR_BINARY.execution.sequence.push_back(
			{
				{0, 1, 2},
				BASIC_INSTRUCTION::BINARY_PURE,
			}
		);

		OPERATOR_BINARY.variables.push_back(first_var);
		OPERATOR_BINARY.variables.push_back(second_var);
		OPERATOR_BINARY.variables.push_back(result);

		code_db.usr_to_function["operator-"] = code_db.functions.size();
		code_db.usr_to_function["operator+"] = code_db.functions.size();
		code_db.usr_to_function["operator<"] = code_db.functions.size();
		code_db.usr_to_function["operator>"] = code_db.functions.size();
		code_db.functions.push_back(OPERATOR_BINARY);
	}

	{
		product_functor
		vector {
		};

		vector.name = "Custom vector";
		vector.number_of_arguments = 1;
		vector.fixed_multipliers = {
			{
				"Custom vector size",
				PRIMITIVE_TYPE
			}
		};
		vector.result.push_back({
			"Custom vector data",
			0
		});

		{
			function_description
			vector_data
			{};

			vector_data.display = "Get vector data";
			vector_data.parameters_count = 1;
			vector_data.is_method = true;

			/*

			Add THIS

			*/

			/*
			variable
			this_param
			= allocate_space(meta->types, method, meta->type_stack.back());

			this_param.is_reference_parameter = true;
			this_param.assumed_type = meta->type_stack.back();
			this_param.name = "this";
			this_param.display = "This";
			*/

			/*

			Create variable which points to location of data

			*/

			vector_data.execution.sequence.push_back({
				{0, 1},
				BASIC_INSTRUCTION::WRITE
			});
		}
	}

	//

	{
		function_description
		m128_assign {
		};

		m128_assign.display = "X Length";
		m128_assign.available_memory_location = 0;

		variable
		first_var
		= allocate_space(code_db.types, m128_assign, PRIMITIVE_TYPE);

		variable
		second_var
		= allocate_space(code_db.types, m128_assign, PRIMITIVE_TYPE);

		m128_assign.is_method = true;
		m128_assign.parameters_count = 1;
		m128_assign.execution.sequence.push_back(
			{
				{0, 1},
				BASIC_INSTRUCTION::WRITE,
			}
		);

		m128_assign.variables.push_back(first_var);
		m128_assign.variables.push_back(second_var);

		code_db.usr_to_function["c:@N@std@ST>2#T#T@vector@F@_Xlength#S"] = code_db.functions.size();
		code_db.functions.push_back(m128_assign);
	}

	//  we hardcode `parallel for` to launch lambdas

	/*
	flow parallel {};
	parallel.name = "parallel_execute";
	parallel.hash = 1;
	parallel.parameters_count = 3;

	point index_start {};
	index_start.name = "pl_start";
	index_start.is_param = true;
	index_start.display_name = "loop start";
	parallel.local_parameters.push_back(code_db.points.size());
	code_db.points.push_back(index_start);

	point index_end {};
	index_end.name = "pl_end";
	index_end.is_param = true;
	index_end.display_name = "loop end";
	int location_end = code_db.points.size();
	parallel.local_parameters.push_back(code_db.points.size());
	code_db.points.push_back(index_end);

	point job {};
	job.name = "job";
	job.is_param = true;
	job.display_name = "job";
	int location_job = code_db.points.size();
	parallel.local_parameters.push_back(code_db.points.size());
	code_db.points.push_back(job);

	point execution {};
	execution.name = "execute";
	execution.display_name = "execute";
	execution.is_function_call = true;
	execution.computation.push_back(location_job);
	execution.computation.push_back(location_end);
	parallel.local_parameters.push_back(code_db.points.size());
	code_db.points.push_back(execution);

	code_db.flows.push_back(parallel);
	*/

	handle_generic_cursor(cursor, &code_db);

	std::cout
		<< "digraph G {"
		<< "fontname=\"Helvetica,Arial,sans-serif\""
		<< "node [fontname=\"Helvetica,Arial,sans-serif\"]"
		<< "edge [fontname=\"Helvetica,Arial,sans-serif\"]"
		<< "graph [ rankdir = \"LR\"]\n";

	int i = 0;
	std::map<int, int> access_count;
	std::vector<std::map<int, int>> memmap {{}};
	std::map<int, std::string> names;
	int memory_offset = 0;
	std::map<size_t, std::string> function_reference {};

	for (auto& flow : code_db.functions) {
		i++;
		if (flow.is_templated) {
			continue;
		}

		// if (flow.name.find("test_apply") != std::string::npos)


		std::map<std::string, int> visited {};

		if (flow.name.find("test_wrapper") != std::string::npos)
		// if (flow.name.find("daily_update") != std::string::npos)
		execute_function(
			names,
			i,
			code_db,
			access_count,
			flow, visited,
			function_reference,
			memmap,
			memory_offset,
			memory_offset
		);
	}

	std::cout
		<< "root [shape=Mdiamond];"
		<< "}";
}