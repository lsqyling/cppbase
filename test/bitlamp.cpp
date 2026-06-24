#include <iostream>
#include <format>


namespace bitlamp {
/*
        项目设计
        我们的俱乐部一共有32栈灯，设计一个灯光控制系统，其中台球部8栈灯，桌游
        区8栈灯，酒吧区8栈灯，休息区8栈灯。

        要求满足以下功能：
        (1）能够独立控制每一栈灯
        (2) 能够一次性打开或关闭一个区域的全部灯光
        (3) 能够获取各个区域的灯光打开关闭情况
        (4) 能够一次性关闭打开的灯打开关闭的灯
*/

unsigned int lamp_state = 0x00'00'00'00;

/*
 * 能够控制每一盏灯。给出所有的灯一个编号0---31 共32盏灯；同时按北区域编号：台球区-0，桌游区-1，酒吧区-2，休息区-3
 */
enum class Area {
    BilliardsArea = 0,
    BoardGameArea,
    BarArea,
    RestArea
};

bool check(int lamp_no) {
    bool r = lamp_state & (1U << lamp_no);
    if (r) {
        std::cout << std::format("第{}号灯是开着的", lamp_no) << std::endl;
    } else {
        std::cout << std::format("第{}号灯是关着的", lamp_no) << std::endl;
    }
    return r;
}

/**
 * (1）能够独立控制每一栈灯
 * @param lamp_no
 */
void switch_lamp(int lamp_no) {
    if (lamp_no < 0 || lamp_no > 31) {
        std::cout << "灯的编号为0 --- 31，请检查你的输入" << std::endl;
    }
    std::cout << "切换前: " << std::endl;
    check(lamp_no);
    lamp_state = lamp_state ^ (1U << lamp_no);
    std::cout << "切换后: " << std::endl;
    check(lamp_no);
}

/**
 * (2) 能够一次性打开或关闭一个区域的全部灯光
 */
void open_lamps_of(Area somewhere) {
    int index = static_cast<int>(somewhere);
    int begin = index * 8;
    int end = index * 8 + 7;
    for (int i = begin; i <= end; ++i) {
        check(i);
    }
    lamp_state |= (0xff << index);
    for (int i = begin; i <= end; ++i) {
        check(i);
    }
}
 void close_lamps_of(Area somewhere) {
     int index = static_cast<int>(somewhere);
     int begin = index * 8;
     int end = index * 8 + 7;
     for (int i = begin; i <= end; ++i) {
         check(i);
     }
     lamp_state ^= (0xff << index);
     for (int i = begin; i <= end; ++i) {
         check(i);
     }
 }

/**
 * (3) 能够获取各个区域的灯光打开关闭情况
 */
void check_Area_of(Area somewhere) {
    int index = static_cast<int>(somewhere);
    int begin = index * 8;
    int end = index * 8 + 7;
    for (int i = begin; i <= end; ++i) {
        check(i);
    }
}
/**
 * (4) 能够一次性关闭打开的灯打开关闭的灯
 */
 void switch_all_lamps() {
     for (int i = 0; i < 32; ++i) {
         check(i);
         switch_lamp(i);
     }
 }

}



int main() {
    using namespace bitlamp;
    bitlamp::switch_all_lamps();
    bitlamp::switch_lamp(22);
    bitlamp::switch_lamp(28);
    bitlamp::open_lamps_of(Area::BarArea);
    bitlamp::check_Area_of(Area::RestArea);
    bitlamp::close_lamps_of(Area::BilliardsArea);

    return 0;
}