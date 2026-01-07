#include "solve.h"
#include "mode.h"
#include "util.h"
#include <string>
#include <vector>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <chrono> // 添加头文件


void solve_serial(std::string format_supply_path, std::string demand_path, std::string heat_path, std::string output_path, solver::ModelMode mode, std::vector<std::string> online_query_list)
{
    solver::sls_model m_model(format_supply_path, demand_path, heat_path);
    m_model.solve_problem(output_path, mode, online_query_list);
}
void test_solve()
{
    std::string demand_path = "/pub/data/lijy/alimama/Local_xunliang/test_data/test_demand_multicomponent.txt";
    std::string format_supply_path = "/pub/data/lijy/alimama/Local_xunliang/test_data/test_supply_multicomponent.txt";
    std::string heat_path = "/pub/data/lijy/alimama/Local_xunliang/test_data/test_heat_multicomponent.txt";
    std::string output_path = "/pub/data/lijy/alimama/Local_xunliang/res/test/test";
    solver::ModelMode mode = solver::Greedy;
    std::vector<std::string> online_query_list={"1","2"};

    solve_serial(format_supply_path, demand_path, heat_path, output_path, mode, online_query_list);
}


int main(int argc, char *argv[])
{
    // 记录程序开始时间
    auto start_time = std::chrono::high_resolution_clock::now();

    // 检查是否是测试模式
    if (argc == 2 && std::string(argv[1]) == "--test") {
        std::cout << "Running in test mode..." << std::endl;
        test_solve();
        
        // 记录程序结束时间
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::cout << "Test mode execution time: " << duration.count() << " milliseconds" << std::endl;
        return 0;
    }

    // 检查参数数量
    if (argc < 5) {
        std::cout << "Usage: " << argv[0] << " <demand_file> <supply_file> <heat_file> <output_file> [online_query_list]" << std::endl;
        std::cout << "       " << argv[0] << " --test   (run in test mode)" << std::endl;
        std::cout << "Example: " << argv[0] << " demand.txt supply.txt heat.txt output 75434962076,75352456666" << std::endl;
        std::cout << "Example: " << argv[0] << " --test" << std::endl;
        return -1;
    }

    solver::ModelMode mode = solver::Greedy; 
    
    // 从命令行参数获取文件路径
    std::string demand_filename = argv[1];
    std::string supply_filename = argv[2];
    std::string heat_file = argv[3];
    std::string output_base = argv[4];  // 输出文件基础路径
    
    // 处理 online_query_list，如果提供了第5个参数
    std::vector<std::string> online_query_list;
    if (argc >= 6) {
        std::string query_string = argv[5];
        // 按逗号分割字符串
        std::vector<std::string> temp_vec;
        solver::split(query_string, temp_vec, ',');
        online_query_list = temp_vec;
    } else {
        // 使用默认值
        online_query_list = { "75434962076", "75352456666", "75330272698", "75329403109"};
    }

    std::cout << "开始串行求解..." << std::endl;
    solve_serial(supply_filename, demand_filename, heat_file, output_base, mode, online_query_list);

    // 记录程序结束时间
    auto end_time = std::chrono::high_resolution_clock::now();

    // 计算并输出运行时间
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Program execution time: " << duration.count() << " milliseconds" << std::endl;

    return 0;
}
