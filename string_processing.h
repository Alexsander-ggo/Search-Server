#pragma once

#include <iostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using SetString = std::set<std::string, std::less<>>;

using VectorStringView = std::vector<std::string_view>;

template <typename StringContainer>
inline SetString MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    SetString non_empty_strings;
    for (std::string_view str : strings) {
        if (!str.empty()) {
            non_empty_strings.emplace(str);
        }
    }
    return non_empty_strings;
}

VectorStringView SplitIntoWords(std::string_view text); 
