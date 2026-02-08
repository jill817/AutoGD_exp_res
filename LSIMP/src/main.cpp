#include "instacne.hpp"
#include "lsearch.hpp"
#include <cstring>
#include "generatelp/generatelp.h"
// 计时相关头文件
#include <chrono>
#include <iostream>
#include "utils.hpp"
bool readLpFile = false;
bool useNewVersion = true;



int main(int argc, char** argv) {
    util::setRandom(DEFAULT_RANDOM_SEED);

    // 记录程序开始时间
    auto program_start = std::chrono::steady_clock::now();
    std::time_t start_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cout << "程序开始运行时间: " << std::ctime(&start_c);

    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <demand_file> <supply_file> <heat_file> <output_base>" << std::endl;
        return 1;
    }

    std::string demand_filename = argv[1];
    std::string supply_filename = argv[2];
    std::string heat_file = argv[3];
    std::string output_base = argv[4];
    std::string lp_path = generate_lp(demand_filename,supply_filename, heat_file, output_base);
    std::cout << "lp_path: " << lp_path << std::endl;

    LS_NIA::NIA_Formula formula = LS_NIA::lpReader::readLpFile(lp_path);

    startTime = util::getTimePoint();
    LS_NIA::LsSolver solver(formula);

    solver.solve(useNewVersion);

    // 记录程序结束时间
    auto program_end = std::chrono::steady_clock::now();
    std::time_t end_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cout << "程序结束运行时间: " << std::ctime(&end_c);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(program_end - program_start).count();
    std::cout << "程序一共运行了: " << duration / 1000.0 << " 秒" << std::endl;
}
