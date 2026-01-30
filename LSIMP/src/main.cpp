#include "instacne.hpp"
#include "lsearch.hpp"
#include <cstring>
#include <iostream>
#include <chrono>
#include "generatelp/generatelp.h"
#include "utils.hpp"
bool readLpFile = false;
bool useNewVersion = true;

int main(int argc, char** argv) {
    util::setRandom(DEFAULT_RANDOM_SEED);

    // 记录程序开始时间
    auto program_start = std::chrono::steady_clock::now();
    std::cout << "程序开始运行时间: ";
    std::time_t start_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cout << std::put_time(std::localtime(&start_time_t), "%Y-%m-%d %H:%M:%S") << std::endl;

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
    std::cout << "程序结束运行时间: ";
    std::time_t end_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cout << std::put_time(std::localtime(&end_time_t), "%Y-%m-%d %H:%M:%S") << std::endl;

    // 计算并输出总运行时长
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(program_end - program_start).count() / 1000.0;
    std::cout << "程序一共运行了: " << duration << " 秒" << std::endl;
}
