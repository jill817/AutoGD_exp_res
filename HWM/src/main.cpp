#include "solve.h"
#include "mode.h"
#include "util.h"
#include <string>
#include <vector>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <chrono> // 添加头文件


void solve_serial(std::string format_supply_path, std::string demand_path, std::string heat_path, std::string output_path, solver::ModelMode mode, int cutoff_seconds, size_t tail_keep)
{
    solver::sls_model m_model(format_supply_path, demand_path, heat_path, tail_keep);
    m_model.solve_problem(output_path, mode, cutoff_seconds);
    // std::cout << "Done." << std::endl;
}
void test_solve()
{
    std::string demand_path = "/pub/data/lijy/alimama/Local_xunliang/test_data/test_demand_multicomponent.txt";
    std::string format_supply_path = "/pub/data/lijy/alimama/Local_xunliang/test_data/test_supply_multicomponent.txt";
    std::string heat_path = "/pub/data/lijy/alimama/Local_xunliang/test_data/test_heat_multicomponent.txt";
    std::string output_path = "/pub/data/lijy/alimama/Local_xunliang/res/test/test";
    solver::ModelMode mode = solver::Greedy;
    // 测试用的在线需求文件与超时时间
    int cutoff_seconds = 10;
    size_t tail_keep = 50;

    solve_serial(format_supply_path, demand_path, heat_path, output_path, mode, cutoff_seconds, tail_keep);
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
        std::cout << "Usage: " << argv[0] << " <demand_file> <supply_file> <heat_file> <output_file> [tail_keep]" << std::endl;
        std::cout << "       " << argv[0] << " --test   (run in test mode)" << std::endl;
        std::cout << "Example: " << argv[0] << " demand.txt supply.txt heat.txt output 50" << std::endl;
        std::cout << "Example: " << argv[0] << " --test" << std::endl;
        return -1;
    }

    solver::ModelMode mode = solver::Greedy; 
    
    // 从命令行参数获取文件路径
    std::string demand_filename = argv[1];
    std::string supply_filename = argv[2];
    std::string heat_file = argv[3];
    std::string output_base = argv[4];  // 输出文件基础路径
    int online_cutoff_seconds = 10;  // 固定超时时间
    size_t tail_keep = 50;    // 默认在线需求截尾数
    if (argc >= 6) {
        tail_keep = std::stoul(argv[5]);
    }

    std::cout << "开始串行求解..." << std::endl;
    solve_serial(supply_filename, demand_filename, heat_file, output_base, mode, online_cutoff_seconds, tail_keep);

    // 记录程序结束时间
    auto end_time = std::chrono::high_resolution_clock::now();

    // 计算并输出运行时间
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Program execution time: " << duration.count() << " milliseconds" << std::endl;

    return 0;
}
