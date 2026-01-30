#include "model.h"
#include "solver.h"
#include "mode.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/stat.h>
#include <cstdlib>
#include <stdexcept>

/**
 * @brief Data Format Description
 * 1. integer dataset:  user id | supply id | cast times | demand_list (split by ,)
 * 2. demand dataset: demand id | demand amount
 * 3. var's name change
 */

namespace solver
{
    void split(std::string &str, std::vector<std::string> &vec, char x)
    {
        vec.clear();
        int start = 0, end = 0;
        for (int i = 0; i < str.length(); i++)
        {
            if (str[i] == x)
            {
                end = i;
                vec.push_back(str.substr(start, end - start));
                start = i + 1;
            }
        }
        end = str.length();
        if (end - start > 0)
        {
            vec.push_back(str.substr(start, end - start));
        }
    }

    sls_model::sls_model(std::string integer_set, std::string demand_set, std::string heat_set) : format_supply_path(integer_set), demand_path(demand_set), heat_path(heat_set) {}

    void sls_model::read_demand_data()
    {
        std::cout << "Reading demand data..." << std::endl;
        demand_id_amount.clear();
        last_demand_id_from_file.clear();
        std::ifstream demand_file(demand_path);
        std::string record;
        std::cout << "Opened demand file: " << demand_path << std::endl;
        while (getline(demand_file, record))
        {
            std::vector<std::string> strs;
            split(record, strs, '`');
            demand_id_amount[strs[0]] = std::stoi(strs[1]);
            if (!strs[0].empty()) {
                last_demand_id_from_file = strs[0]; // 记录文件顺序的最后一行需求 id
            }
        }
        std::cout << "demand data size: " << demand_id_amount.size() << std::endl;
        demand_file.close();
        long long total_demand = 0;
        for (const auto& pair : demand_id_amount) {
            total_demand += pair.second;
        }

        std::cout << "总原始需求量: " << total_demand << std::endl;
    }

    void sls_model::read_heat_data()
    {
        user_supply_heat.clear(); // 清空之前的热度数据
        std::ifstream heat_file(heat_path); // 假设 heat_path 是存储热度数据的文件路径
        if (!heat_file.is_open()) {
            std::cerr << "无法打开热度数据文件: " << heat_path << std::endl;
            return;
        }

        std::string record;
        while (std::getline(heat_file, record)) {
            std::vector<std::string> strs;
            split(record, strs, '`'); // 使用相同的分隔符解析每一行

            if (strs.size() >= 3) { // 确保每一行有足够的字段
                std::string user_id = strs[0];
                std::string supply_id = strs[1];
                double heat_coefficient = std::stod(strs[2]);

                // 将热度系数存入 user_supply_heat 数据结构
                user_supply_heat[user_id][supply_id] = heat_coefficient;
            } else {
                std::cerr << "警告：跳过格式不正确的行 - " << record << std::endl;
            }
        }

        heat_file.close();
    }


    void sls_model::read_integer_data()
    {
        user_supply_cast_times.clear();
        user_supply_demand_set.clear();
        supply_user_demand_set.clear();
        supply_demand_set.clear();
        supply_with_more_demand.clear();
        std::ifstream integer_file(format_supply_path);
        std::string record;
        // user`supply`demand1,demand2
        while (getline(integer_file, record))
        {
            // only read ?%
            // if (rand() % 10000 < 5000)
            // {
            //     continue;
            // }
            std::vector<std::string> strs;
            split(record, strs, '`');
            user_supply_cast_times[strs[0]][strs[1]] = std::stoi(strs[2]);
            std::vector<std::string> demand_list;
            split(strs[3], demand_list, ',');
            for (auto demand_id : demand_list)
            {
                if (user_supply_demand_set[strs[0]][strs[1]].count(demand_id) == 0)
                {
                    user_supply_demand_set[strs[0]][strs[1]].insert(demand_id);
                }
                if (supply_user_demand_set[strs[1]][strs[0]].count(demand_id) == 0)
                {
                    supply_user_demand_set[strs[1]][strs[0]].insert(demand_id);
                }
                if (supply_demand_set[strs[1]].count(demand_id) == 0)
                {
                    supply_demand_set[strs[1]].insert(demand_id);
                }
            }
        }
        for (auto ele : supply_demand_set)
        {
            if (ele.second.size() >= 2)
            {
                supply_with_more_demand.insert(ele.first);
            }
        }
        integer_file.close();
    }

    /**
     * All ids or strs are used with converted ids
     */
    void sls_model::model_problem(std::string lp_file, int nia_num, std::string result_file, ModelMode mode)
    {
        std::cout << "user num | supply num | demand num\n";
        std::cout << user_supply_cast_times.size() << " | " << supply_demand_set.size() << " | " << demand_id_amount.size() << std::endl;
        
        // rand();//nor for original
        bool enable_given_query = false;
        bool enable_rand_query = false;
        bool enable_last_query = false;
        bool enable_max_query = false;
        if (mode == GivenQueryObj)
        {
            enable_given_query = true;
            std::cout << "需要修改给定查询的需求节点，请在代码中修改给定查询的需求节点。" << std::endl;
        }
        else
        {
            if (mode == RandQueryObj)
            {
                enable_rand_query = true;
            }
            else if (mode == LastQueryObj)
            {
                enable_last_query = true;
            }
            else if (mode == MaxQueryObj)
            {
                enable_max_query = true;
            }
        }
        std::unordered_set<std::string> query_strs;
        if(enable_given_query)
        {
            std::vector<std::string> given_query_demands = {""}; 
            for (const auto& given_query_demand : given_query_demands)
            {
                for (auto it = demand_id_convert.begin(); it != demand_id_convert.end(); it++)
                {
                    if (given_query_demand == it->first)
                    {
                        query_strs.insert(it->second);
                        break;
                    }
                }
            }
            std::cout << "Using given query selection." << std::endl;
            std::cout << "Selected query strings: ";
            for (const auto& query_str : query_strs)
            {
                std::cout << query_str << " ";
            }
            std::cout << std::endl;
        }
        else 
        {
            // Use random query selection
            if (enable_rand_query)
            {
                // rand();
                int query_id = rand() % demand_id_convert.size();
                // Random a query demand node
                int cnt = 0;
                for (auto it = demand_id_convert.begin(); it != demand_id_convert.end(); it++)
                {
                    if (cnt == query_id)
                    {
                        // query_str = it->second;
                        query_strs.insert(it->second);
                        break;
                    }
                    cnt++;
                }
                std::cout << "Using random query selection." << std::endl;
                std::cout << "Selected query strings: ";
                for (const auto& query_str : query_strs)
                {
                    std::cout << query_str << " ";
                }
                std::cout << std::endl;
            }
            else if (enable_last_query)
            {
                // Pick the demand node that appears on the last line of the demand file
                auto it = demand_id_convert.find(last_demand_id_from_file);
                if (it != demand_id_convert.end())
                {
                    query_strs.insert(it->second);
                    std::cout << "Using last-line demand selection." << std::endl;
                }
                else
                {
                    throw std::runtime_error("last demand id from file not found in id mapping; aborting LastQueryObj selection");
                }
                std::cout << "Selected query strings: ";
                for (const auto& query_str : query_strs)
                {
                    std::cout << query_str << " ";
                }
                std::cout << std::endl;
            }
            else if (enable_max_query)
            {
                // Demand with Maximum var count
                rand();
                std::unordered_map<std::string, int> demand_var_count;
                for (auto ele1 : user_supply_demand_set)
                {
                    auto user_id = ele1.first;
                    for (auto ele2 : ele1.second)
                    {
                        auto supply_id = ele2.first;
                        for (auto demand_id : ele2.second)
                        {
                            demand_var_count[demand_id]++;
                        }
                    }
                }
                int max_count = 0;
                std::vector<int> demand_nums;
                for (auto ele : demand_var_count)
                {
                    demand_nums.push_back(ele.second);
                    if (ele.second > max_count)
                    {
                        max_count = ele.second;
                        // query_str = ele.first;
                        query_strs.insert(ele.first);
                    }
                }
                std::cout << demand_nums.size() << std::endl;
                sort(demand_nums.begin(), demand_nums.end());
                reverse(demand_nums.begin(), demand_nums.end());
                int top_n_count = demand_nums[10];
                std::cout << top_n_count << std::endl;
                for (auto ele : demand_var_count)
                {
                    if (ele.second == top_n_count)
                    {
                        // query_str = ele.first;
                        query_strs.insert(ele.first);
                    }
                }
                std::cout << "Using variable count-based query selection." << std::endl;
                std::cout << "Selected query strings: ";
                for (const auto& query_str : query_strs)
                {
                    std::cout << query_str << " ";
                }
                std::cout << std::endl;
            }
            else
            {
                std::cout << "No query selection mode specified, using empty query." << std::endl;
            }
        }

        var_name_index.clear();
        demand_vars.clear();
        // register vars
        for (auto ele1 : user_supply_demand_set)
        {
            auto user_id = ele1.first;
            for (auto ele2 : ele1.second)
            {
                auto supply_id = ele2.first;
                for (auto demand_id : ele2.second)
                {
                    std::string var_name = "x_" + user_id_convert[user_id] + "_" + supply_id_convert[supply_id] + "_" + demand_id_convert[demand_id];
                    double var_coeff = 0.0;

                    // 如果是查询字符串对应的需求节点，则系数为 -1.0
                    if (enable_given_query && query_strs.count(demand_id_convert[demand_id]))
                    {
                        var_coeff = -1.0;
                    }
                    else if (enable_rand_query && query_strs.count(demand_id_convert[demand_id]))
                    {
                        var_coeff = -1.0;
                    }
                    else if (enable_last_query && query_strs.count(demand_id_convert[demand_id]))
                    {
                        var_coeff = -1.0;
                    }
                    else if (enable_max_query && query_strs.count(demand_id_convert[demand_id]))
                    {
                        var_coeff = -1.0;
                    }
                    else  // 如果不是查询字符串对应的需求节点，则系数为 0
                    {
                        var_coeff = 0.0;
                    }

                    int var_index = m_sls_solver.register_var(var_name, var_coeff, true, true, 0, user_supply_cast_times[user_id][supply_id]);
                    var_name_index[var_name] = var_index;
                    demand_vars[demand_id].push_back(var_index);
                }
                // 从 user_supply_heat 中获取对应的热度系数
                double var_coeff = 0.0;
                if (mode == HeatObj) {
                    if (user_supply_heat.find(user_id) != user_supply_heat.end() && user_supply_heat[user_id].find(supply_id) != user_supply_heat[user_id].end()) {
                        var_coeff = user_supply_heat[user_id][supply_id]; // 获取热度值
                    } else {
                        std::cerr << "警告：未找到用户 " << user_id << " 和供应方 " << supply_id << " 的热度值，使用默认值 0.0\n";
                    }
                }
                else if (mode == ZeroObj) {
                    var_coeff = 0.0;
                }
                // std::string supply_var_name = "x_" + user_id_convert[user_id] + "_" + supply_id_convert[supply_id];
                // int supply_var_index = m_sls_solver.register_var(supply_var_name, var_coeff, true, true, 0, user_supply_cast_times[user_id][supply_id]);
                // var_name_index[supply_var_name] = supply_var_index;

            }
        }

        // std::unordered_map<std::string, int> slack_vars;
        // double BIG_COEFFICIENT = 31708040;
        // for (auto ele : demand_vars) {
        //     auto demand_id = ele.first;
        //     std::string slack_var_name = "p_" + demand_id_convert[demand_id];
        //     int slack_var_index = m_sls_solver.register_var(slack_var_name, BIG_COEFFICIENT, true, true, 0, INFINITY);
        //     slack_vars[demand_id] = slack_var_index;
        // }
        

        std::cout << "register vars done\n";

        // Part I. Basic Constraints: Linear
        // 2.1 user-supply: sum of vars <= cast count
        // for (auto ele1 : user_supply_demand_set)
        // {
        //     auto user_id = ele1.first;
        //     for (auto ele2 : ele1.second)
        //     {
        //         auto supply_id = ele2.first;
        //         int cast_count = user_supply_cast_times[user_id][supply_id];
        //         // if(cast_count>50) {
        //         //     std::cout<<user_id<<std::endl;
        //         //     std::cout<<supply_id<<std::endl;
        //         //     std::cout<<" >50 "<<std::endl;
        //         // }
        //         monomial_vector m_monomials;
        //         for (auto demand_id : ele2.second)
        //         {
        //             std::string var_name = "x_" + user_id_convert[user_id] + "_" + supply_id_convert[supply_id] + "_" + demand_id_convert[demand_id];
        //             int_table vars;
        //             vars.insert(var_name_index[var_name]);
        //             m_monomials.push_back(monomial(1.0, vars));
        //         }
        //         polynomial poly(m_monomials);
        //         m_sls_solver.register_constraint(poly, constraint_kind::CAST, "user_supply_" + user_id + "_" + supply_id, false, true, 0, cast_count);
        //     }
        // }
        // std::cout << "2.1 supply done\n";

        // 2.1 user-supply: sum of vars <= cast count；sum of vars = x_user_supply
        for (auto ele1 : user_supply_demand_set)
        {
            auto user_id = ele1.first;
            for (auto ele2 : ele1.second)
            {
                auto supply_id = ele2.first;
                int supply_var_index = var_name_index["x_" + user_id_convert[user_id] + "_" + supply_id_convert[supply_id]];

                monomial_vector m_monomials;

                // 第一条约束：需求变量的总和等于新的变量 x_user_supply
                for (auto demand_id : ele2.second)
                {
                    std::string var_name = "x_" + user_id_convert[user_id] + "_" + supply_id_convert[supply_id] + "_" + demand_id_convert[demand_id];
                    int_table vars;
                    vars.insert(var_name_index[var_name]);
                    m_monomials.push_back(monomial(1.0, vars));
                }
                int_table supply_vars;
                supply_vars.insert(supply_var_index);
                m_monomials.push_back(monomial(-1.0, supply_vars)); // 添加新的变量到约束中
                polynomial poly1(m_monomials);
                m_sls_solver.register_constraint(poly1, constraint_kind::CAST, "user_supply_sum_" + user_id + "_" + supply_id, true, true, 0, 0);

                // 第二条约束：新的变量 x_user_supply <= cast_count
                // monomial_vector m_supply_monomials;
                // int_table single_supply_var;
                // single_supply_var.insert(supply_var_index);
                // m_supply_monomials.push_back(monomial(1.0, single_supply_var));
                // polynomial poly2(m_supply_monomials);
                // m_sls_solver.register_constraint(poly2, constraint_kind::CAST, "user_supply_bound_" + user_id + "_" + supply_id, false, true, 0, user_supply_cast_times[user_id][supply_id]);

                monomial_vector m_supply_monomials;
                for (auto demand_id : ele2.second)
                {
                    std::string var_name = "x_" + user_id_convert[user_id] + "_" + supply_id_convert[supply_id] + "_" + demand_id_convert[demand_id];
                    int_table vars;
                    vars.insert(var_name_index[var_name]);
                    m_supply_monomials.push_back(monomial(1.0, vars));
                }
                polynomial poly2(m_supply_monomials);
                m_sls_solver.register_constraint(poly2, constraint_kind::CAST, "user_supply_" + user_id + "_" + supply_id, false, true, 0, user_supply_cast_times[user_id][supply_id]);

                }
        }
        std::cout << "2.1 supply done\n";

        // // 2.2 demand: sum of vars >= demand need
        // for (auto ele : demand_vars)
        // {
        //     monomial_vector m_monomials;
        //     auto demand_id = ele.first;
        //     if (demand_id == query_str)
        //     {
        //         continue;
        //     }
        //     int demand_amount = demand_id_amount[demand_id];
        //     for (auto m_var : ele.second)
        //     {
        //         int_table vars;
        //         vars.insert(m_var);
        //         m_monomials.push_back(monomial(1.0, vars));
        //     }
        //     polynomial poly(m_monomials);
        //     m_sls_solver.register_constraint(poly, constraint_kind::DEMAND, "demand_" + demand_id, true, false, demand_amount, 0);
        // }

        // 2.2 demand: sum of vars + slack var >= demand need
        for (auto ele : demand_vars) {
            monomial_vector m_monomials;
            auto demand_id = ele.first;
            int demand_amount = demand_id_amount[demand_id];
        
            // 添加原有变量
            for (auto m_var : ele.second) {
                int_table vars;
                vars.insert(m_var);
                m_monomials.push_back(monomial(1.0, vars));
            }
        
            // // 添加辅助变量
            // int_table slack_vars_table;
            // slack_vars_table.insert(slack_vars[demand_id]);
            // m_monomials.push_back(monomial(1.0, slack_vars_table));
        
            polynomial poly(m_monomials);
            m_sls_solver.register_constraint(poly, constraint_kind::DEMAND, "demand_" + demand_id, true, false, demand_amount, 0);
        }
        
        std::cout << "2.2 demand done\n";


        // Part II. Advanced Constraints: Multilinear
        // 3.7 random choose a supply node and two demand nodes
        // int num_3_7 = nia_num;
        // supply_demands_set m_supply_demands;
        // for (int k = 0; k < num_3_7; k++)
        // {
        //     std::string supply_str, demand_str1, demand_str2;
        //     int supply_id, demand_id1, demand_id2;
        //     while (true)
        //     { // in case we do not random to a right node
        //         // 1. random choose a supply node
        //         int supply_size = supply_with_more_demand.size();
        //         supply_id = rand() % supply_size;
        //         int cnt = 0;
        //         for (auto it = supply_with_more_demand.begin(); it != supply_with_more_demand.end(); it++)
        //         {
        //             if (cnt == supply_id)
        //             {
        //                 supply_str = *it;
        //             }
        //             cnt++;
        //         }
        //         ASSERT(supply_demand_set[supply_str].size() >= 2);
        //         // 2. random choose two demand nodes attached to this supply node
        //         do
        //         {
        //             demand_id1 = rand() % supply_demand_set[supply_str].size();
        //             demand_id2 = rand() % supply_demand_set[supply_str].size();
        //         } while (demand_id1 == demand_id2);
        //         cnt = 0;
        //         supply_demands *curr_supply_demand = new supply_demands(supply_id, demand_id1, demand_id2);
        //         if (m_supply_demands.count(curr_supply_demand) != 0)
        //         {
        //             continue;
        //         }
        //         m_supply_demands.insert(curr_supply_demand);

        //         for (auto it = supply_demand_set[supply_str].begin(); it != supply_demand_set[supply_str].end(); it++)
        //         {
        //             if (cnt == demand_id1)
        //             {
        //                 demand_str1 = *it;
        //             }
        //             if (cnt == demand_id2)
        //             {
        //                 demand_str2 = *it;
        //             }
        //             cnt++;
        //         }
        //         break;
        //     }
        //     std::cout << "3.7 supply id | 3.7 demand id1 | 3.7 demand id2\n";
        //     std::cout << supply_str << " | " << demand_str1 << " | " << demand_str2 << std::endl;
        //     // 3. generate constraint
        //     /*
        //         sum_i x{ui, s, d1} / sum_ij x{ui, sj, d1} >= sum_i x{ui, s, d2} / sum_ij x{ui, sj, d2}
        //         -------------------------------------------------------------------------------------
        //         sum_i x{ui, s, d1} * sum_ij x{ui, sj, d2} >= sum_i x{ui, s, d2} * sum_ij x{ui, sj, d1}
        //         ------------------   --------------------    ------------------   --------------------
        //             p1                     p4                     p2                    p3
        //     */
        //     // loop all edges
        //     monomial_vector p1s, p2s, p3s, p4s;
        //     int_table m_vars;
        //     for (auto ele1 : user_supply_demand_set)
        //     {
        //         auto curr_user_id = ele1.first;
        //         for (auto ele2 : ele1.second)
        //         {
        //             auto curr_supply_id = ele2.first;
        //             for (auto curr_demand_id : ele2.second)
        //             {
        //                 if (curr_supply_id == supply_str && curr_demand_id == demand_str1)
        //                 { // p1 case
        //                     m_vars.clear();
        //                     m_vars.insert(var_name_index["x_" + user_id_convert[curr_user_id] + "_" + supply_id_convert[curr_supply_id] + "_" + demand_id_convert[curr_demand_id]]);
        //                     p1s.push_back(monomial(1.0, m_vars));
        //                 }
        //                 if (curr_supply_id == supply_str && curr_demand_id == demand_str2)
        //                 { // p2 case
        //                     m_vars.clear();
        //                     m_vars.insert(var_name_index["x_" + user_id_convert[curr_user_id] + "_" + supply_id_convert[curr_supply_id] + "_" + demand_id_convert[curr_demand_id]]);
        //                     p2s.push_back(monomial(1.0, m_vars));
        //                 }
        //                 if (curr_demand_id == demand_str1)
        //                 { // p3 case
        //                     m_vars.clear();
        //                     m_vars.insert(var_name_index["x_" + user_id_convert[curr_user_id] + "_" + supply_id_convert[curr_supply_id] + "_" + demand_id_convert[curr_demand_id]]);
        //                     p3s.push_back(monomial(1.0, m_vars));
        //                 }
        //                 if (curr_demand_id == demand_str2)
        //                 { // p4 case
        //                     m_vars.clear();
        //                     m_vars.insert(var_name_index["x_" + user_id_convert[curr_user_id] + "_" + supply_id_convert[curr_supply_id] + "_" + demand_id_convert[curr_demand_id]]);
        //                     p4s.push_back(monomial(1.0, m_vars));
        //                 }
        //             }
        //         }
        //     }
        //     polynomial p1(p1s), p2(p2s), p3(p3s), p4(p4s);
        //     // std::cout << "four polys' size: " << p1.size() << ", " << p2.size() << ", " << p3.size() << ", " << p4.size() << std::endl;
        //     p1.mul(p4);
        //     p2.mul(p3);
        //     // std::cout << "left and right poly's size: " << p1.size() << ", " << p2.size() << std::endl;
        //     polynomial cons_poly = p1 - p2;
        //     // std::cout << "cons size: " << cons_poly.size() << std::endl;
        //     m_sls_solver.register_constraint(cons_poly, constraint_kind::DEMAND_COMPARE, "cons_3_7" + supply_str + "_" + demand_str1 + "_" + demand_str2, true, 0, false, 0);
        // }

        std::cout << "start solve\n";
        m_sls_solver.write_lp_file(lp_file);
        std::cout << "write lp file done\n";
        // 调用 Gurobi 求解器
        // solve_with_gurobi(lp_file, mode);
    }

    void sls_model::generate_id_convert()
    {
        user_id_convert.clear();
        supply_id_convert.clear();
        demand_id_convert.clear();
        int idx;
        for (auto ele1 : user_supply_demand_set)
        {
            auto user_id = ele1.first;
            if (user_id_convert.count(user_id) == 0)
            {
                idx = user_id_convert.size();
                user_id_convert[user_id] = std::to_string(idx);
            }
            for (auto ele2 : ele1.second)
            {
                auto supply_id = ele2.first;
                if (supply_id_convert.count(supply_id) == 0)
                {
                    idx = supply_id_convert.size();
                    supply_id_convert[supply_id] = std::to_string(idx);
                }
                for (auto demand_id : ele2.second)
                {
                    if (demand_id_convert.count(demand_id) == 0)
                    {
                        idx = demand_id_convert.size();
                        demand_id_convert[demand_id] = std::to_string(idx);
                    }
                }
            }
        }
    }

    void sls_model::solve_problem(std::string lp_file, int nia_num, std::string result_file, ModelMode mode)
    {
        read_demand_data();
        read_integer_data();
        read_heat_data();
        generate_id_convert();
        model_problem(lp_file, nia_num, result_file,mode);
    }
};