#include <iostream>
#include <format>
#include <vector>
#include <array>
#include <map>

namespace array {
/*
 * 易道云学院，今天开始入学，要求登记每一位报到的学生，因此我们要设计一个程序，要求输入学员的编号，并且打印出来
 */
struct Stu
{
    std::string id;
    std::string name;
    std::string stu_name;
};

Stu all[1001];
std::map<std::string, int> map_to_pos;
int len = 0;

void registerStu() {
    std::string id, name, stu_name;
    std::cin >> id >> name >> stu_name;
    all[len] = Stu{id, name, stu_name};
    map_to_pos[id] = len++;
}

void printID(std::string id) {
    auto index = map_to_pos[id];
    std:: cout << std::format("{{\"{}\", {}, \"{}\"}}", all[index].id, all[index].name, all[index].stu_name) << std::endl;
}
}






int main()
{
    const int b = 1;
    int *ptrb = const_cast<int *>(&b);
    *ptrb = 2;
    std::cout << *ptrb << std::endl;
    std::cout << b << std::endl;
    int arr[2][5]{0};
    int (*pa)[5] = arr;
    return 0;
}