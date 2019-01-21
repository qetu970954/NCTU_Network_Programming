#ifndef NETWORKPROGRAMMING_MYTYPEDEFINE_H
#define NETWORKPROGRAMMING_MYTYPEDEFINE_H

#include <map>
#include <string>
#include <queue>
#include <vector>

template<typename T>
using Vector = std::vector<T>;

template<typename T>
using Deque = std::deque<T>;

template<typename T>
using Queue = std::queue<T>;

template<typename T1, typename T2>
using Map = std::map<T1, T2>;

using String = std::string;

template<typename T1, typename T2>
using Pair = std::pair<T1, T2>;
#endif //NETWORKPROGRAMMING_MYTYPEDEFINE_H
