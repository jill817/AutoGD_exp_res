#include "model.h"
#include "solver.h"
#include "mode.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/stat.h>
#include <cstdlib>
#include <sstream>
#include <limits>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <regex>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <chrono>

// 反向映射表
std::unordered_map<std::string, std::string> user_id_convert_rev;
std::unordered_map<std::string, std::string> supply_id_convert_rev;
std::unordered_map<std::string, std::string> demand_id_convert_rev;

/**
 * @brief Data Format Description
 * 1. integer dataset:  user id | supply id | cast times | demand_list (split by ,)
 * 2. demand dataset: demand id | demand amount
 * 3. var's name change
 */

namespace solver
{
    // 直接用内存数据结构进行RAP求解，无需LP文件

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
        demand_id_amount.clear();
        demand_read_order.clear();
        last_valid_demand_id.clear();
        std::ifstream demand_file(demand_path);
        std::string record;
        while (getline(demand_file, record))
        {
            std::vector<std::string> strs;
            split(record, strs, '`');
            demand_id_amount[strs[0]] = std::stoi(strs[1]);
            demand_read_order.push_back(strs[0]);
            // demand_id_amount[strs[0]] = std::stoi(strs[1]) * 1.0 / 50000; // 5w original
        }
        if (!demand_read_order.empty()) {
            last_valid_demand_id = demand_read_order.back();
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
    void sls_model::model_problem(std::string lp_file, std::string output_base, int nia_num, std::string result_file, ModelMode mode,int time_limit, int online_top_k)
    {
        // (void)online_top_k; // online_top_k stored on model; here for signature consistency
        std::cout << "user num | supply num | demand num\n";
        std::cout << user_supply_cast_times.size() << " | " << supply_demand_set.size() << " | " << demand_id_amount.size() << std::endl;
        
        // rand();//nor for original
        bool enable_given_query = false;
        bool enable_rand_query = false;
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
            for (auto ele2 : ele1.second
            )
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
                std::string supply_var_name = "x_" + user_id_convert[user_id] + "_" + supply_id_convert[supply_id];
                int supply_var_index = m_sls_solver.register_var(supply_var_name, var_coeff, true, true, 0, user_supply_cast_times[user_id][supply_id]);
                var_name_index[supply_var_name] = supply_var_index;

            }
        }
        std::unordered_map<std::string, int> slack_vars;
        double BIG_COEFFICIENT = 31708040;
        for (auto ele : demand_vars) {
            auto demand_id = ele.first;
            std::string slack_var_name = "p_" + demand_id_convert[demand_id];
            int slack_var_index = m_sls_solver.register_var(slack_var_name, BIG_COEFFICIENT, true, true, 0, INFINITY);
            slack_vars[demand_id] = slack_var_index;
        }
        

        std::cout << "register vars done\n";


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
        
            // 添加辅助变量
            int_table slack_vars_table;
            slack_vars_table.insert(slack_vars[demand_id]);
            m_monomials.push_back(monomial(1.0, slack_vars_table));
        
            polynomial poly(m_monomials);
            m_sls_solver.register_constraint(poly, constraint_kind::DEMAND, "demand_" + demand_id, true, false, demand_amount, 0);
        }
        
        std::cout << "2.2 demand done\n";


        std::cout << "start solve (in-memory)\n";
        // m_sls_solver.write_lp_file(lp_file); // 注释掉LP文件写入
        // std::cout << "write lp file done\n";
        // solve_with_rap(lp_file, sol_file, mode, std::chrono::steady_clock::now(), time_limit); // 注释掉LP解析
        solve_with_rap_from_model(m_sls_solver, output_base, mode, std::chrono::steady_clock::now(), time_limit, online_top_k);
        // std::cout<<"test3"<<std::endl;
    }

    void sls_model::generate_id_convert()
    {
        user_id_convert.clear();
        supply_id_convert.clear();
        demand_id_convert.clear();
            user_id_convert_rev.clear();
            supply_id_convert_rev.clear();
            demand_id_convert_rev.clear();
        int idx;
        for (auto ele1 : user_supply_demand_set)
        {
            auto user_id = ele1.first;
            if (user_id_convert.count(user_id) == 0)
            {
                idx = user_id_convert.size();
                user_id_convert[user_id] = std::to_string(idx);
                    user_id_convert_rev[std::to_string(idx)] = user_id;
            }
            for (auto ele2 : ele1.second)
            {
                auto supply_id = ele2.first;
                if (supply_id_convert.count(supply_id) == 0)
                {
                    idx = supply_id_convert.size();
                    supply_id_convert[supply_id] = std::to_string(idx);
                        supply_id_convert_rev[std::to_string(idx)] = supply_id;
                }
                for (auto demand_id : ele2.second)
                {
                    if (demand_id_convert.count(demand_id) == 0)
                    {
                        idx = demand_id_convert.size();
                        demand_id_convert[demand_id] = std::to_string(idx);
                            demand_id_convert_rev[std::to_string(idx)] = demand_id;
                    }
                }
            }
        }
    }

    void sls_model::solve_problem(std::string lp_file,std::string output_base, int nia_num, std::string result_file, ModelMode mode,int time_limit, int online_top_k)
    {
        // this->online_top_k = online_top_k;
        read_demand_data();
        read_integer_data();
        read_heat_data();
        generate_id_convert();
        init_allocation_state();
        model_problem(lp_file,output_base, nia_num, result_file,mode,time_limit, online_top_k);
    }

    void sls_model::init_allocation_state()
    {
        sid_index.clear();
        did_index.clear();
        index_sid.clear();
        index_did.clear();
        sid_pv.clear();
        sid_remainpv.clear();
        sid_did.clear();
        sid_did_allocatepv.clear();
        did_remain_amount.clear();

        int sid_counter = 0;
        int did_counter = 0;

        // 先为所有 demand 建索引（仅限于在 supply 出现的需求）
        for (const auto &ele : supply_demand_set) {
            for (const auto &did : ele.second) {
                if (did_index.count(did) == 0) {
                    did_index[did] = did_counter++;
                    index_did.push_back(did);
                }
            }
        }

        // 构建 sid 及容量、可投放集合
        for (const auto &u_entry : user_supply_demand_set) {
            const auto &user_id = u_entry.first;
            for (const auto &s_entry : u_entry.second) {
                const auto &supply_id = s_entry.first;
                std::string sid = user_id + "_" + supply_id;
                if (sid_index.count(sid) == 0) {
                    sid_index[sid] = sid_counter++;
                    index_sid.push_back(sid);
                    sid_pv.push_back(0);
                    sid_remainpv.push_back(0);
                    sid_did.emplace_back();
                    sid_did_allocatepv.emplace_back();
                }
                int sid_idx = sid_index[sid];
                int capacity = user_supply_cast_times[user_id][supply_id];
                sid_pv[sid_idx] += capacity;
                sid_remainpv[sid_idx] += capacity;

                for (const auto &did : s_entry.second) {
                    auto it = did_index.find(did);
                    if (it == did_index.end()) continue;
                    sid_did[sid_idx].insert(it->second);
                }
            }
        }

        // 初始化需求剩余量
        did_remain_amount.resize(did_index.size(), 0);
        for (const auto &kv : did_index) {
            const std::string &did = kv.first;
            int idx = kv.second;
            if (demand_id_amount.count(did)) {
                did_remain_amount[idx] = demand_id_amount[did];
            }
        }

        // 清空指标
        origin_gain_online = 0;
        unsatisfied_total = 0;
        penalty = 0.0;
        gain_metric = 0.0;
    }

    void sls_model::reset_allocation_state()
    {
        sid_remainpv = sid_pv;
        did_remain_amount.assign(did_index.size(), 0);
        for (const auto &kv : did_index) {
            const std::string &did = kv.first;
            int idx = kv.second;
            auto it = demand_id_amount.find(did);
            if (it != demand_id_amount.end()) {
                did_remain_amount[idx] = it->second;
            }
        }
        for (auto &mp : sid_did_allocatepv) {
            mp.clear();
        }
    }

    std::vector<std::string> sls_model::choose_online_demand_ids(int online_top_k) const
    {
        std::vector<std::string> targets;
        if (online_top_k <= 0) {
            return targets;
        }

        int remaining = online_top_k;
        for (auto it = demand_read_order.rbegin(); it != demand_read_order.rend() && remaining > 0; ++it) {
            if (did_index.count(*it)) {
                targets.push_back(*it);
                --remaining;
            }
        }
        return targets;
    }

    void sls_model::map_solution_to_allocation(const std::vector<std::string>& var_list, const std::vector<double>& x, int online_top_k)
    {
        reset_allocation_state();
        std::vector<std::string> online_targets = choose_online_demand_ids(online_top_k);
        std::unordered_set<std::string> online_target_set(online_targets.begin(), online_targets.end());
        for (size_t i = 0; i < var_list.size() && i < x.size(); ++i) {
            int alloc = static_cast<int>(std::round(x[i]));
            if (alloc <= 0) continue;
            const std::string &var = var_list[i];
            if (var.size() < 3 || var[0] != 'x' || var[1] != '_') continue;
            std::vector<std::string> parts;
            std::string var_copy = var;
            split(var_copy, parts, '_');
            if (parts.size() != 4) continue; // 只处理 x_user_supply_demand
            const std::string user_orig = user_id_convert_rev[parts[1]];
            const std::string supply_orig = supply_id_convert_rev[parts[2]];
            const std::string demand_orig = demand_id_convert_rev[parts[3]];
            if (!online_target_set.empty() && online_target_set.count(demand_orig)) {
                continue; // leave for online phase
            }
            std::string sid = user_orig + "_" + supply_orig;
            auto sid_it = sid_index.find(sid);
            auto did_it = did_index.find(demand_orig);
            if (sid_it == sid_index.end() || did_it == did_index.end()) continue;
            int sid_idx = sid_it->second;
            int did_idx = did_it->second;
            sid_did_allocatepv[sid_idx][did_idx] += alloc;
            sid_remainpv[sid_idx] = std::max(0, sid_remainpv[sid_idx] - alloc);
            did_remain_amount[did_idx] = std::max(0, did_remain_amount[did_idx] - alloc);
        }
    }

    void sls_model::LSout_offline(const std::string& offline_res_file)
    {
        std::ofstream ofs(offline_res_file);
        if (!ofs.is_open()) {
            std::cerr << "无法打开文件 " << offline_res_file << " 进行写入。" << std::endl;
            return;
        }
        ofs << "SID,Demand,Allocated PV\n";
        for (size_t sid = 0; sid < sid_did_allocatepv.size(); ++sid) {
            const std::string &sid_str = index_sid[sid];
            for (const auto &kv : sid_did_allocatepv[sid]) {
                int did_idx = kv.first;
                int pv = kv.second;
                const std::string &did_str = index_did[did_idx];
                ofs << sid_str << "," << did_str << "," << pv << "\n";
            }
        }
        ofs.close();
        std::cout << "分配方案已输出到: " << offline_res_file << std::endl;
    }

    void sls_model::mark_fixed_online_demand(int online_top_k)
    {
        did_online_mask.assign(did_index.size(), 0);
        std::vector<std::string> targets = choose_online_demand_ids(online_top_k);
        for (const auto& target : targets) {
            if (did_index.count(target)) {
                did_online_mask[did_index[target]] = 1;
            }
        }

        if (!targets.empty()) {
            std::cout << "Online demand selected: ";
            for (size_t i = 0; i < targets.size(); ++i) {
                std::cout << targets[i] << (i + 1 == targets.size() ? "" : ", ");
            }
            std::cout << std::endl;
        } else {
            std::cout << "Warning: no valid demand available for online allocation." << std::endl;
        }
    }

    void sls_model::FIFO_online(int cutoff_seconds)
    {
        using clock = std::chrono::steady_clock;
        auto time_start = clock::now();
        size_t processed_supply = 0;
        int timed_out = 0;

        for (int sid = 0; sid < static_cast<int>(sid_remainpv.size()); ++sid) {
            if (++processed_supply % 1000 == 0) {
                auto now = clock::now();
                double elapsed = std::chrono::duration<double>(now - time_start).count();
                if (elapsed > cutoff_seconds) {
                    std::cerr << "[警告] 在线分配达到超时 " << cutoff_seconds << " 秒，已处理 supply 数: " << processed_supply << std::endl;
                    timed_out = 1;
                    break;
                }
            }
            int &remain = sid_remainpv[sid];
            if (remain <= 0) continue;

            for (int did : sid_did[sid]) {
                if (did >= static_cast<int>(did_online_mask.size())) continue;
                if (did_online_mask[did] == 0) continue;
                int allocate = remain;
                sid_did_allocatepv[sid][did] += allocate;
                origin_gain_online += allocate;

                int demand_before = did_remain_amount[did];
                int actual_satisfied = std::min(allocate, demand_before);
                did_remain_amount[did] = demand_before - actual_satisfied;
                remain = 0;
                break;
            }
        }

        if (!timed_out) {
            std::cout << "在线 FIFO 分配完成。" << std::endl;
        }
    }

    void sls_model::LSout_online(const std::string& online_res_file)
    {
        std::ofstream fout(online_res_file);
        if (!fout.is_open()) {
            std::cerr << "无法打开文件 " << online_res_file << " 进行写入。" << std::endl;
            return;
        }
        fout << "sid,demand,allocated_pv\n";
        for (size_t sid = 0; sid < sid_did_allocatepv.size(); ++sid) {
            const std::string &sid_str = index_sid[sid];
            for (const auto &kv : sid_did_allocatepv[sid]) {
                int did = kv.first;
                int pv = kv.second;
                const std::string &did_str = index_did[did];
                fout << sid_str << "," << did_str << "," << pv << "\n";
            }
        }
        fout.close();
        std::cout << "Online allocation results in file " << online_res_file << "\n";
    }

    void sls_model::compute_and_log_online_metrics()
    {
        long long unsat = 0;
        for (int rem : did_remain_amount) {
            unsat += rem;
        }
        double pen = static_cast<double>(unsat) * 1000.0;
        double gain = static_cast<double>(origin_gain_online) - pen;
        unsatisfied_total = unsat;
        penalty = pen;
        gain_metric = gain;
        std::cout << "Online metrics: origin_gain=" << origin_gain_online
                  << ", penalty=" << pen
                  << ", gain=" << gain
                  << ", unsatisfied_total=" << unsat << std::endl;
        std::cout << "OBJ: " << gain << std::endl;
        // std::cout<< "test1"<<std::endl;
    }

    void sls_model::compute_and_log_offline_demand_totals(int online_top_k) const
    {
        std::vector<std::string> online_targets = choose_online_demand_ids(online_top_k);
        std::unordered_set<std::string> online_set(online_targets.begin(), online_targets.end());

        long long offline_total = 0;           // 所有非 online demand 的需求总量（不要求出现在 supply）
        long long offline_total_effective = 0; // 所有非 online 且出现在 supply(did_index) 的需求总量

        for (const auto &kv : demand_id_amount) {
            const std::string &did = kv.first;
            int amount = kv.second;
            if (!online_set.empty() && online_set.count(did)) {
                continue;
            }
            offline_total += static_cast<long long>(amount);
            if (did_index.count(did)) {
                offline_total_effective += static_cast<long long>(amount);
            }
        }

        std::cout << "Offline demand totals (exclude online_top_k=" << online_top_k << "): "
                  << "offline_total=" << offline_total
                  << ", offline_total_effective=" << offline_total_effective
                  << std::endl;
    }



    void sls_model::solve_with_rap_from_model(const solver::opt_solver& solver, const std::string& output_base, ModelMode mode, const std::chrono::steady_clock::time_point& start_time, int time_limit, int online_top_k)
    {
        // (void)online_top_k; // already stored in member in solve_problem
        const double alpha0 = 0.00001;
        const double tol = 1e-6;
        const double inf_bound = 1e8;

        // 1. 提取变量信息
        const auto& vars = solver.get_vars();
        int n = (int)vars.size();
        std::vector<std::string> var_list(n);
        std::vector<double> c(n), w(n), lb(n), ub(n);
        for (int i = 0; i < n; ++i) {
            var_list[i] = vars[i].m_name;
            c[i] = vars[i].m_coeff;
            w[i] = 0.0; // 若有二次项可补充
            lb[i] = vars[i].m_lb;
            ub[i] = vars[i].m_ub;
        }

        // 2. 提取约束信息
        const auto& constraints = solver.get_constraints();
        std::vector<std::vector<double>> A_eq, A_ineq;
        std::vector<double> b_eq, b_ineq;
        for (const auto* cons : constraints) {
            // 只处理线性约束
            std::vector<double> coeffs(n, 0.0);
            for (const auto& mono : cons->m_poly.m_monomials) {
                if (mono.m_vars.size() == 1) {
                    int var_idx = *(mono.m_vars.begin());
                    if (var_idx >= 0 && var_idx < n) {
                        coeffs[var_idx] += mono.m_coeff;
                    }
                }
                // 多元项可忽略或报错
            }
            // 等式/不等式
            if (cons->m_lower && cons->m_upper && cons->m_lower_bound == cons->m_upper_bound) {
                A_eq.push_back(coeffs);
                b_eq.push_back(cons->m_lower_bound);
            } else {
                if (cons->m_lower) {
                    A_ineq.push_back(coeffs);
                    b_ineq.push_back(cons->m_lower_bound);
                }
                if (cons->m_upper) {
                    for (auto& v : coeffs) v = -v;
                    A_ineq.push_back(coeffs);
                    b_ineq.push_back(-cons->m_upper_bound);
                }
            }
        }

        // 3. 稀疏化
        auto dense_to_sparse_rows = [&](const std::vector<std::vector<double>>& A_dense)
            -> std::vector<std::vector<std::pair<int,double>>> {
            std::vector<std::vector<std::pair<int,double>>> rows;
            rows.reserve(A_dense.size());
            for (const auto &row : A_dense) {
                std::vector<std::pair<int,double>> srow;
                for (int j = 0; j < (int)row.size(); ++j) {
                    double v = row[j];
                    if (v != 0.0) srow.emplace_back(j, v);
                }
                rows.emplace_back(std::move(srow));
            }
            return rows;
        };
        std::vector<std::vector<std::pair<int,double>>> Aeq_rows = dense_to_sparse_rows(A_eq);
        std::vector<std::vector<std::pair<int,double>>> Aineq_rows = dense_to_sparse_rows(A_ineq);

        int m_eq = (int)A_eq.size();
        int m_ineq = (int)A_ineq.size();

        // 4. 初始化变量
        std::vector<double> x(n, 0.0);
        for (int j=0;j<n;++j) {
            if (std::isfinite(lb[j]) && lb[j] > -inf_bound/2) x[j] = lb[j];
            else x[j] = 0.0;
        }
        std::vector<double> mu(m_ineq, 0.0);
        std::vector<double> lam(m_eq, 0.0);
        std::vector<double> r = c;

        std::cout << "[RAP] (in-memory) 开始对偶子梯度迭代（时间限制："<<time_limit<<"秒）" << std::endl;
        for (int t = 0; ; ++t) {
            double alpha = alpha0 / std::sqrt((double)(t + 1));
            std::vector<double> x_prev = x;
            for (int j = 0; j < n; ++j) {
                if (w[j] > 0.0) {
                    double zj = - r[j] / w[j];
                    if (zj < lb[j]) zj = lb[j];
                    if (zj > ub[j]) zj = ub[j];
                    x[j] = zj;
                } else {
                    if (r[j] > 0.0) x[j] = lb[j];
                    else if (r[j] < 0.0) x[j] = ub[j];
                    else x[j] = x_prev[j];
                }
            }

            // 单次稀疏行遍历：计算 s = A * x - b，并直接把对偶增量的 A^T * delta 加到 r
            std::vector<double> s_eq(m_eq, 0.0);
            std::vector<double> s_ineq(m_ineq, 0.0);
            for (int i = 0; i < m_eq; ++i) {
                double Ax = 0.0;
                const auto &row = Aeq_rows[i];
                for (const auto &p : row) Ax += p.second * x[p.first];
                double si = Ax - b_eq[i];
                s_eq[i] = si;
                double delta_lam = alpha * si;
                lam[i] += delta_lam;
                if (delta_lam != 0.0) {
                    for (const auto &p : row) r[p.first] += p.second * delta_lam;
                }
            }
            for (int i = 0; i < m_ineq; ++i) {
                double Ax = 0.0;
                const auto &row = Aineq_rows[i];
                for (const auto &p : row) Ax += p.second * x[p.first];
                double si = Ax - b_ineq[i];
                s_ineq[i] = si;
                double mu_old = mu[i];
                double mu_candidate = mu_old + alpha * si;
                if (mu_candidate < 0.0) mu_candidate = 0.0;
                double delta_mu = mu_candidate - mu_old;
                if (delta_mu != 0.0) {
                    for (const auto &p : row) r[p.first] += p.second * delta_mu;
                    mu[i] = mu_candidate;
                }
            }
            double max_eq_viol = 0.0;
            for (int i = 0; i < m_eq; ++i) max_eq_viol = std::max(max_eq_viol, std::fabs(s_eq[i]));
            double max_ineq_viol = 0.0;
            for (int i = 0; i < m_ineq; ++i) {
                double viol = std::max(0.0, -s_ineq[i]);
                if (viol > max_ineq_viol) max_ineq_viol = viol;
            }
            if (t % 10 == 0) {
                double quad = 0.0;
                for (int j = 0; j < n; ++j) quad += w[j] * x[j] * x[j];
                double obj = 0.5 * quad;
                for (int j = 0; j < n; ++j) obj += c[j] * x[j];
                std::cout << "[RAP] iter=" << t << " obj=" << obj
                        << " max_eq_viol=" << max_eq_viol
                        << " max_ineq_viol=" << max_ineq_viol << std::endl;
                auto current_time = std::chrono::steady_clock::now();
                auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
                if (elapsed_time >= time_limit) {
                    std::cout << "[RAP] 时间限制已到，提前终止迭代。" << std::endl;
                    break;
                }
            }
            if (max_eq_viol < tol && max_ineq_viol < tol) {
                std::cout << "[RAP] 收敛于 iter=" << t << ", eq_viol=" << max_eq_viol << ", ineq_viol=" << max_ineq_viol << std::endl;
                break;
            }
        }
        // 输出离线结果并进入在线阶段
        map_solution_to_allocation(var_list, x,online_top_k);
        std::string offline_csv = output_base + "_offline.csv";
        LSout_offline(offline_csv);

        mark_fixed_online_demand(online_top_k);
        compute_and_log_offline_demand_totals(online_top_k);
        FIFO_online(time_limit);
        std::string online_csv = output_base + "_online.csv";
        LSout_online(online_csv);
        compute_and_log_online_metrics();
        // std::cout<< "test2"<<std::endl;
    }

};