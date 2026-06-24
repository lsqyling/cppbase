#include <iostream>

class obj
{
    int a{0};
    int b{0};
    void say()
    {
        std::cout << "hello obj." << std::endl;
    }
};

void test()
{
    obj obj_1, obj_2;
    obj_1 = obj_2;
}

class Base
{
public:
    Base()
    {
        std::cout << "Base constructor" << std::endl;
    }
    ~Base()
    {
        std::cout << "Base dtor" << std::endl;
    }
};

class Derived : public Base
{
public:
    Derived()
    {
        std::cout << "Derived constructor" << std::endl;
    }
    ~Derived()
    {
        std::cout << "Derived dtor" << std::endl;
    }
};

void test1()
{
    // 默认调用基类默认的构造函数
    Derived derived;
    // 离开作用域时，析构顺序和构造顺序相反，先本类后基类
}






int main()
{
    test();
    test1();
    return 0;
}