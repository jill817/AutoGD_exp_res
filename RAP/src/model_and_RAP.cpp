#include "sls_solver/model.h"
#include "sls_solver/mode.h"
#include <string>
#include <vector>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>


void solve(std::string format_supply_path, std::string demand_path, std::string heat_path,std::string lp_path,solver::ModelMode mode,int time_limit,std::string output_base)
{
    int nia_num=100;
    solver::sls_model m_model(format_supply_path, demand_path, heat_path);
    std::cout<<"start solve"<<std::endl;
    m_model.solve_problem(lp_path, output_base,
                            nia_num, 
                            "experiment/" + std::to_string(nia_num) + ".res", mode,time_limit);
}


int main(int argc, char *argv[])
{
    // supply 格式id1`id2`capacity`demand1,demand2,...   
    // demand 格式 demand_id`demand_amount
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <demand_file> <supply_file> <heat_file> <output_base>" << std::endl;
        return 1;
    }
    solver::ModelMode mode = solver::HeatObj; 

    std::string demand_filename = argv[1];
    std::string supply_filename = argv[2];
    std::string heat_file = argv[3];
    std::string output_base = argv[4];

    // 解析 time_limit 参数
    int time_limit=60;

    std::string lp_path = output_base + "_model.lp";
    solve(supply_filename, demand_filename, heat_file, lp_path, mode, time_limit,output_base);

    return 0;
}
