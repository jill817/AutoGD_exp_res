#ifndef GENERATELP_GENERATELP_H
#define GENERATELP_GENERATELP_H

#include <string>

#include "sls_solver/mode.h"

void test_write_lp(std::string format_supply_path,
				   std::string demand_path,
				   std::string heat_path,
				   std::string lp_path);

void test_solve_demo(std::string format_supply_path,
					 std::string demand_path,
					 std::string heat_path,
					 std::string lp_path);

void solve(std::string format_supply_path,
		   std::string demand_path,
		   std::string heat_path,
		   std::string lp_path,
		   solver::ModelMode mode);

std::string generate_lp(const std::string& demand_filename,
						const std::string& supply_filename,
						const std::string& heat_file,
						const std::string& output_base);

#endif // GENERATELP_GENERATELP_H
