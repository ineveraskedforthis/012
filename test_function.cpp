/*
int add(int a, int b) {
        return a + b;
}

int super_add(int a, int b) {
        int x = add(a, b);
        return x;
}

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

template<typename F>
void apply_x(sys::state& state, F f) {
        state.quality1 = f(state.quality1);
}

void update(sys::state& state1, sys::state& state2) {
        state1.quality1 = state2.quality3 + state2.world.data2[2];
        state2.world.data3[5] = state2.world.data3[4] * state2.world.data1[1];
        if (state1.quality1 > 5) {
                state1.quality1 = state1.quality1 + 2;
        }

        for (auto i = 5; i < 10; i++) {
                state1.quality2 += state2.quality2;
        }

        apply_x(state1, [&](auto x) {return x + 1;});
}

template<typename G, typename F>
int apply(G g, F f) {
        return f(g);
}

int test() {
        return apply(1, [&](auto x){return x + 1;});
}
*/
/*
template<typename T>
struct stupid_struct{
        T data;
        T get_data(){
                return data;
        }
};



int test_struct() {
        stupid_struct<float> x = {5};
        x.data = 10;
        return x.get_data();
}
*/

struct smart_struct{
        int data;
        int get_data(){
                return data;
        }
};

int test_struct2() {
        smart_struct x = {5};
        if (x.data == 6) {
                x.data = 10;
        }
        return x.get_data();
}
