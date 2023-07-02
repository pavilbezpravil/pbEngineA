#include <iostream>
#include <thread>
#include <concepts>
#include <vector>
#include <array>
#include <algorithm>
#include <numeric>
#include <ranges>


using std::vector;
using std::array;

namespace r = std::ranges;
namespace rv = std::ranges::views;

#define INFO(x) std::cout << x << std::endl

void Sleep(int ms) {
   std::this_thread::sleep_for(std::chrono::milliseconds(ms));
   // _sleep(ms);
}

// void Add(auto a) {
//    if constexpr (std::is_same_v<decltype(a), int>) {
//       INFO(a + 1);
//    } else {
//       INFO(a);
//    }
// }

// void Add(auto a) {
//    if constexpr (std::same_as<decltype(a), int>) {
//       INFO(a + 1);
//    }
//    else {
//       INFO(a);
//    }
// }

template <typename T> requires std::same_as<T, int>
void Add(T a) {
   INFO(a + 1);
}

template <typename T> requires std::integral<T> || std::floating_point<T>
constexpr float Average(const vector<T>& v) {
   float sum = std::accumulate(v.begin(), v.end(), 0.f);
   return sum / v.size();
}


template <typename T, size_t N, size_t... Idxs>
constexpr std::array<T, N> __make_array(const T(&l)[N], std::index_sequence<Idxs...>) {
   return {l[Idxs]...};
}

template <typename T, size_t N>
constexpr auto make_array(const T (&l)[N]){
   return __make_array(l, std::make_index_sequence<N>{});
}


template <typename T>
struct remove_const {
   using type = T;
};


template <typename T>
struct remove_const<const T> {
   using type = T;
};


template <typename T>
using remove_const_t = remove_const<T>::type;


template <typename T>
struct remove_reference {
   using type = T;
};


template <typename T>
struct remove_reference<T&> {
   using type = T;
};


template <typename T>
struct remove_reference<T&&> {
   using type = T;
};


template <typename T>
using remove_reference_t = remove_reference<T>::type;


template<typename, typename>
constexpr bool is_same_v = false;

template<typename T>
constexpr bool is_same_v<T, T> = true;

template <bool Value>
struct negation {
   static constexpr bool value = !Value;
};


// AND
template <bool Value>
struct conjunction {
   static constexpr bool value = !Value;
};

// OR
// template <bool FirstValue, bool First, bool ...Rest>
// struct _disjunction<FirstValue, First, Rest...> {
//    static constexpr bool value = disjunction<Rest...>::value;
// };
//
// template <bool FirstValue, bool First, bool ...Rest>
// struct _disjunction<true, First, Rest...> {
//    static constexpr bool value = true;
// };
//
// template <bool ...Ts>
// struct disjunction {
//    static constexpr bool value = false;
// };
//
// template <bool First, bool ...Rest>
// struct disjunction<First, Rest...> {
//    static constexpr bool value = disjunction<Rest...>::value;
// };



#define STYPE_EQ(T1, T2) static_assert(is_same_v<T1, T2>)

int main() {
   INFO("Hello, World!");

   STYPE_EQ(int, int);
   STYPE_EQ(int, std::remove_const_t<const int>);
   STYPE_EQ(int, remove_const_t<int>);
   STYPE_EQ(int, remove_const_t<const int>);
   STYPE_EQ(int, remove_reference_t<int&>);
   STYPE_EQ(int, remove_reference_t<int&&>);

   // static_assert(!disjunction<>::value);
   // static_assert(disjunction<true>::value);

   constexpr bool a = negation<true>::value;
   INFO(a);

   // constexpr float avg = Average(vector<int>{ 1, 2, 3, 4, 5 });
   // std::array<int, 5> v{ { 1, 2, 3, 4, 5 } };
   // constexpr float avg = Average(v);

   // constexpr size_t s = std::initializer_list<int>{ 1, 2, 3 }.size();
   // INFO(s);

   // auto arr = make_array({1, 2, 3});
   // for (auto i : arr) {
   //    INFO(i);
   // }

   //
   // for (auto i : arr | rv::reverse) {
   // rv::zip(arr, arr);
   // for (auto i : std::vector<int>{ 1, 2, 3 } | rv::reverse) {
   //    INFO(i);
   // }

   Sleep(1000);
   return 0;
}
