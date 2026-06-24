#include <memory>
#include <iostream>
#include <format>
#include <locale>

/*
 * 设计一个程序，接受用户输入的字符串，并且求这个字符串的长度！
 * 使用char类型来接受用户输入，用户可以输入中文和英文以及数字！
 *
 * 假设用户输入你好123
 * 我们的程序要输出5
 */
/**
 * 检查UTF-8字符的字节数
 */
int getUtf8CharLen(unsigned char c)
{
    if ((c & 0x80) == 0) {
        return 1; // ASCII字符（0xxxxxxx）
    }
    else if ((c & 0xE0) == 0xC0) {
        return 2; // 2字节UTF-8字符（110xxxxx）
    }
    else if ((c & 0xF0) == 0xE0) {
        return 3; // 3字节UTF-8字符（1110xxxx）
    }
    else if ((c & 0xF8) == 0xF0) {
        return 4; // 4字节UTF-8字符（11110xxx）
    }
    return 1; // 默认情况
}

/**
 * 计算UTF-8字符串的字符数（不是字节数）
 */
int calcUtf8Length(const char *str)
{
    int len = 0;
    int i = 0;
    while (str[i]) {
        int charLen = getUtf8CharLen(str[i]);
        i += charLen;
        ++len;
    }
    return len;
}

void test_input_char_len()
{
    char input[1024];
    std::cout << "请输入字符串：";
    std::cin.getline(input, sizeof(input));
    std::cout << calcUtf8Length(input) << std::endl;
}

template<typename T>
void printStr(T &ptr) {
    std::string s;
    std::cin >> s;
    auto b = s + ptr;
    std::cout << b;
    std::cout << ptr << std::endl;
}

// 测试栈溢出
void deep_recursive(int n)
{
    if (n == 0) {
        return ;
    }
    deep_recursive(n - 1);
}
void test_deep_recursive()
{
    deep_recursive(100000);
}

void largeArray() {
    int arr[1000000]; // 分配过大的局部数组，可能导致栈溢出
    arr[0] = 1; // 防止编译器优化
    std::cout << "Array allocated." << std::endl;
}

void test_large_array()
{
    largeArray();
}

void test_ptr()
{
    auto p1 = std::make_unique<int>(3);
    std::cout << *p1 << std::endl;

}

using pointer = int *;



int main()
{
//    test_input_char_len();
    std::string a{"123456"};
    std::cout << typeid("1234").name() << std::endl;
//    printStr("abc");
//    test_deep_recursive(); // 栈溢出
//    test_large_array(); // 栈溢出
    test_ptr();

    std::cout << pointer{} << std::endl;
    std::cout <<  nullptr << std::endl;

    return 0;
}





