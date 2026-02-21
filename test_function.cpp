namespace random_namespace {
static int side_value;
}

void function_with_side_effect() {
        random_namespace::side_value++;
}

namespace cool_functions {
int test_function_1(int a, int b) {
        int c = a + b;
        random_namespace::side_value = a - b;
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

void update(sys::state& state) {
        state.quality1 = state.quality3 + state.world.data2[2];
        state.world.data3[5] = state.world.data3[4] * state.world.data1[1];
}