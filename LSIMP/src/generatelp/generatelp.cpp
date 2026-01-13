#include "sls_solver/model.h"
#include "sls_solver/mode.h"
#include <string>
#include <vector>
#include <iostream>
// # include "graph_connect/graph_connect.h"
// #include "generate_heat/generate_R.h"
#include <sys/stat.h>
#include <sys/types.h>


// void test_write_lp(std::string format_supply_path, std::string demand_path, std::string heat_path,std::string lp_path)
// {
//     solver::sls_model m_model(format_supply_path, demand_path, heat_path);
//     std::string str = "demo_lp.lp";
//     m_model.demo_write_lp(str);
// }

// void test_solve_demo(std::string format_supply_path, std::string demand_path, std::string heat_path,std::string lp_path)
// {
//     solver::sls_model m_model(format_supply_path, demand_path, heat_path);
//     m_model.demo_solve("demo_solve.lp");
// }

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
    // supply 格式id1`id2`capacity`demand1,demand2,...   
    // demand 格式 demand_id`demand_amount
    // if (argc < 5) {
    //     std::cerr << "Usage: " << argv[0] << " <demand_file> <supply_file> <heat_file> <output_base>" << std::endl;
    //     return 1;
    // }
    solver::ModelMode mode = solver::LastQueryObj; 

    // 你可以根据 output_base 构造 lp_path
    std::string lp_path = output_base + ".lp";
    std::cout<< "demand_filename:"<<demand_filename<<std::endl;
    std::cout<< "supply_filename:"<<supply_filename<<std::endl;
    std::cout<< "heat_file:"<<heat_file<<std::endl;

    solve(supply_filename, demand_filename, heat_file, lp_path,mode);

    return lp_path;
}
