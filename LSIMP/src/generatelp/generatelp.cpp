#include "sls_solver/model.h"
#include "sls_solver/mode.h"
#include <string>
#include <vector>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>



void solve(std::string format_supply_path, std::string demand_path, std::string heat_path,std::string lp_path,solver::ModelMode mode)
{
    
    int nia_num=100;
    solver::sls_model m_model(format_supply_path, demand_path, heat_path);
    std::cout<<"start solve"<<std::endl;
    m_model.solve_problem(lp_path, 
                            nia_num, 
                            "experiment/" + std::to_string(nia_num) + ".res", mode);
}


std::string generate_lp(const std::string& demand_filename,
                const std::string& supply_filename,
                const std::string& heat_file,
                const std::string& output_base)
{

    solver::ModelMode mode = solver::LastQueryObj; 

    // 你可以根据 output_base 构造 lp_path
    std::string lp_path = output_base + ".lp";
    std::cout<< "demand_filename:"<<demand_filename<<std::endl;
    std::cout<< "supply_filename:"<<supply_filename<<std::endl;
    std::cout<< "heat_file:"<<heat_file<<std::endl;

    solve(supply_filename, demand_filename, heat_file, lp_path,mode);

    return lp_path;
}
