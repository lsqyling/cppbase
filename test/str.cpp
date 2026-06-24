#include <iostream>
#include <string>
#include <string_view>
#include <format>
#include <vector>
#include <algorithm>

namespace str {
bool is_whitespace_only(std::string_view token)
{
    return std::all_of(token.begin(), token.end(), [](unsigned char c) {
        return std::isspace(c);
    });
}
/**
 * 单字符分割
 */
std::vector<std::string> split(const std::string &text, char delimiter, bool skip_empty = true)
{
    std::vector<std::string> tokens;
    std::string token;
    for (auto c : text) {
        if (c == delimiter) {
            if (!skip_empty || !is_whitespace_only(token)) {
                tokens.push_back(token);
            }
            token.clear();
        } else {
            token += c;
        }
    }
    if (!skip_empty || !is_whitespace_only(token)) {
        tokens.push_back(token);
    }
    return tokens;
}

/**
 * 字符串分割
 * @param text
 * @param delimiter
 * @param skip_empty
 * @return
 */
std::vector<std::string> split(const std::string &text, const std::string &delimiter, bool skip_empty = true)
{
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = text.find(delimiter);

    while (end != std::string::npos) {
        auto token = text.substr(start, end - start);
        if (!skip_empty || !is_whitespace_only(token)) {
            tokens.push_back(token);
        }
        start = end + delimiter.size();
        end = text.find(delimiter, start);
    }
    std::string token = text.substr(start);
    if (!skip_empty || !is_whitespace_only(token)) {
        tokens.push_back(token);
    }

    return tokens;
}

void test_split()
{
    std::string text{"apple, banana, cherry, , , "};
    auto result = split(text, ',');
    for (auto &item : result) {
        std::cout << item << " ";
    }
    std::cout << std::endl;
    auto result1 = split(text, ",");
    for (auto &item : result1) {
        std::cout << item << " ";
    }
    std::cout << std::endl;
    std::cout << result.size() << std::endl;
    std::cout << result1.size() << std::endl;
}
}

struct node
{
    int size{0};
    int data[5];
};

void test_node()
{
    node a{5, {1, 2, 3,4,5}};
    node b{a};
    for (int i = 0; i < 5; ++i) {
        std::cout << b.data[i] << " ";
        std::cout << a.data[i] << " ";
    }
    std::cout << std::endl;
}






int main()
{
    str::test_split();
    int &&r = 3 + 2;
    std::cout << &r << std::endl;
    r = 6;
    std::cout << &r << std::endl;
    std::cout << r << std::endl;
    std::cout << &"abc" << std::endl;
    std::string s;
    test_node();
    return 0;
}