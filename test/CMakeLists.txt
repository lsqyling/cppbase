# 收集所有测试源文件
file(GLOB_RECURSE test_list
        LIST_DIRECTORIES false
        CONFIGURE_DEPENDS *.cpp)

# GTest 相关的包含路径
set(GTEST_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/extern/benchmark/googletest/googletest/include)
set(GMOCK_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/extern/benchmark/googletest/googlemock/include)

foreach (v_ ${test_list})
    get_filename_component(target_name ${v_} NAME_WE)
    add_executable("${target_name}" ${v_})
    
    # 默认链接库
    target_link_libraries(${target_name} PRIVATE cppbase benchmark::benchmark)
    
    # GTest 测试：链接 gtest_main 并添加 include 路径
    if(target_name MATCHES "_gtest$")
        target_link_libraries(${target_name} PRIVATE gtest_main)
        target_include_directories(${target_name} PRIVATE ${GTEST_INCLUDE_DIR} ${GMOCK_INCLUDE_DIR})
    endif()
    
    # Benchmark 测试：已有 benchmark::benchmark 链接
endforeach ()
