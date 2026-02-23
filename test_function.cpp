int add(int a, int b) {
        return a + b;
}

int super_add(int a, int b) {
        int x = add(a, b);
        return x;
}

/*
namespace random_namespace {
static int side_value;
}

void function_with_side_effect() {
        random_namespace::side_value++;
}
void function_with_side_effect2() {
        random_namespace::side_value+=1;
}



namespace cool_functions {
int test_function_1(int a, int b) {
        int c = a + b;
        random_namespace::side_value = a - b;
        function_with_side_effect();
        return c;
}
int test_function_2(int a, int b) {
        int c = add(a, b);
        random_namespace::side_value = add(a ,-b);
        function_with_side_effect();
        return c;
}
}

namespace lame_functions{
void lame_function() {
        function_with_side_effect();
}
}

struct container {
        int* data1;
        float* data2;
        unsigned int* data3;
};

namespace sys {
struct state {
        container world;
        float quality1;
        float quality2;
        float quality3;
};
}

void update(sys::state& state1, sys::state& state2) {
        state1.quality1 = state2.quality3 + state2.world.data2[2];
        // state.world.data3[5] = state.world.data3[4] * state.world.data1[1];
}

*/