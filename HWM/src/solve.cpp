#include "solve.h"

#include "mode.h"

#include <iostream>
#include <fstream>
#include <chrono> // 添加头文件
#include <algorithm>
#include <random>


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

        sls_model::sls_model(std::string integer_set, std::string demand_set, std::string heat_set, size_t tail_keep_)
            : format_supply_path(integer_set), demand_path(demand_set), heat_path(heat_set),
                tail_keep(tail_keep_),
                last_valid_demand_idx(-1), last_valid_demand_id(""),
                origin_gain_online(0), unsatisfied_total(0), penalty(0.0), gain_metric(0.0) {}

    void sls_model::solve_problem(std::string allocation_res_file, ModelMode mode, int cutoff_seconds)
    {
        std::cout << std::endl;
        read_supply_data(); // 内部已经包含两次过滤逻辑：先预加载demand，再过滤supply
        read_demand_data(); // 后读取 demand 文件
        read_heat_data();
        std::cout << std::endl;
        origin_gain_online = 0;
        unsatisfied_total = 0;
        penalty = 0.0;
        gain_metric = 0.0;
        model_problem(allocation_res_file, mode, cutoff_seconds);
        // std::cout << "solve_problem 调用完成，未析构"<<std::endl;
    }

    void sls_model::read_demand_data()
    {
        did_amount.clear();
        did_amount.resize(did_index.size(), 0); 
        did_remain_amount.clear();
        did_remain_amount.resize(did_index.size(), 0);
        last_valid_demand_idx = -1;
        last_valid_demand_id.clear();
        last_online_list.clear();
        last_online_idlist.clear();

        std::ifstream demand_file(demand_path);
        std::string record;

        // 记录所有在 supply 过滤集合中的需求出现顺序，便于截取末尾 N 个
        std::vector<int> valid_order_idx;
        std::vector<std::string> valid_order_id;

        int line_num = 0;
        int valid_matched_demands = 0;  // 既在supply又在demand中的需求数量
        
        while (getline(demand_file, record))
        {
            std::vector<std::string> strs;
            split(record, strs, '`');
            const std::string& demand_id = strs[0];
            const int amount = std::stoi(strs[1]);

            // 检查 demand_id 是否在 did_index 中（即在经过过滤的supply需求中）
            if (did_index.find(demand_id) != did_index.end())
            {
                int demand_idx = did_index[demand_id];
                did_amount[demand_idx] = amount; // 更新对应索引的需求量
                did_remain_amount[demand_idx] = amount; // 初始化剩余需求量
                valid_matched_demands++;
                valid_order_idx.push_back(demand_idx);
                valid_order_id.push_back(demand_id);
                // record the last valid demand that also exists in supply-filtered set
                last_valid_demand_idx = demand_idx;
                last_valid_demand_id = demand_id;
            }
            line_num++;
        }

        // 保留末尾最多 tail_keep 个有效需求（按文件出现顺序）
        if (!valid_order_idx.empty()) {
            size_t start = (valid_order_idx.size() > tail_keep) ? (valid_order_idx.size() - tail_keep) : 0;
            for (size_t i = start; i < valid_order_idx.size(); ++i) {
                last_online_list.push_back(valid_order_idx[i]);
                last_online_idlist.push_back(valid_order_id[i]);
            }
        }

        std::cout << "Valid demand count (both in supply and demand): " << valid_matched_demands << std::endl;
        std::cout << "Online demand tail size (<=50): " << last_online_list.size() << std::endl;
        
        // 检查是否有在supply中但不在demand文件中的需求
        if (valid_matched_demands < (int)did_index.size()) {
            std::cout << "Warning: " << (did_index.size() - valid_matched_demands) 
                      << " demands exist in supply but not in demand file (these were filtered out)" << std::endl;
        }
        
        demand_file.close();
    }

    void sls_model::read_heat_data()
    {
        sid_heat.clear(); // 清空之前的热度数据

        std::ifstream heat_file(heat_path); // 假设 heat_path 是存储热度数据的文件路径
        if (!heat_file.is_open()) {
            std::cerr << "无法打开热度数据文件: " << heat_path << std::endl;
            return;
        }

        // 直接根据 sid_index 的大小调整 sid_heat 的大小，避免循环中的动态调整
        sid_heat.resize(sid_index.size());

        std::string record;
        while (std::getline(heat_file, record)) {
            std::vector<std::string> strs;
            split(record, strs, '`'); // 使用相同的分隔符解析每一行

            if (strs.size() >= 3) { // 确保每一行有足够的字段
                const std::string user_id = strs[0];
                const std::string supply_id = strs[1];
                const double heat_coefficient = std::stod(strs[2]);

                // 构造 sid（user_id 和 supply_id 的组合）
                const std::string sid = user_id + "_" + supply_id;

                // 检查 sid 是否存在于 sid_index 中
                if (sid_index.find(sid) == sid_index.end()) {
                    // std::cerr << "Error: sid " << sid << " not found in sid_index.\n";
                    continue;
                }

                // 使用 sid_index 获取整数索引，无需检查大小
                int sid_idx_mapped = sid_index[sid];
                
                // 存储热度系数
                sid_heat[sid_idx_mapped] = heat_coefficient;
            } else {
                std::cerr << "警告：跳过格式不正确的行 - " << record << std::endl;
            }
        }

        std::cout << "supply heat data size: " << sid_heat.size() << std::endl;
        heat_file.close();
    }

    std::unordered_set<std::string> sls_model::preload_valid_demands()
    {
        std::unordered_set<std::string> valid_demands;
        std::ifstream demand_file(demand_path);
        
        if (!demand_file.is_open()) {
            std::cerr << "无法打开需求数据文件: " << demand_path << std::endl;
            return valid_demands;
        }
        
        std::string record;
        int total_demands = 0;
        
        while (getline(demand_file, record)) {
            std::vector<std::string> strs;
            split(record, strs, '`');
            
            if (strs.size() >= 2) {
                const std::string& demand_id = strs[0];
                valid_demands.insert(demand_id);
                total_demands++;
            }
        }
        
        demand_file.close();
        std::cout << "预加载有效需求: " << valid_demands.size() << " 个需求ID" << std::endl;
        return valid_demands;
    }

    void sls_model::read_supply_data()
    {
        sid_pv.clear();
        sid_did.clear();
        did_sid.clear();
        // canallocate_did_sid.clear();
        sid_index.clear();
        index_sid.clear();
        did_index.clear(); // 初始化 did_index
        index_did.clear(); // 初始化 index_did

        // 首先预加载有效的 demand 集合
        std::unordered_set<std::string> valid_demands = preload_valid_demands();
        std::cout << "开始读取supply数据，将只处理有效需求..." << std::endl;

        std::ifstream supply_file(format_supply_path);
        std::string record;
        int sid_idx = 0; // 用于生成整数索引
        int did_idx = 0; // 用于生成 demand 索引
        int total_demands_in_supply = 0;  // 统计supply中出现的总需求数
        int valid_demands_count = 0;     // 统计有效需求数

        while (getline(supply_file, record))
        {
            std::vector<std::string> strs;
            split(record, strs, '`');
            const std::string user_id = strs[0];
            const std::string supply_id = strs[1];
            const std::string sid = user_id + "_" + supply_id;

        int current_pv = std::stoi(strs[2]);
        int sid_idx_mapped;
        
        if (sid_index.find(sid) == sid_index.end())
        {
            // 首次出现的supply，创建新索引
            sid_index[sid] = sid_idx;
            if (sid_idx >= index_sid.size())
            {
                index_sid.resize(sid_idx + 1);
            }
            index_sid[sid_idx] = sid;
            sid_idx_mapped = sid_idx;
            sid_idx++;
            
            // 扩展向量大小 - 添加边界检查
            if (sid_idx_mapped >= (int)sid_pv.size())  // 添加类型转换
            {
                sid_pv.resize(sid_idx_mapped + 1);
                sid_did.resize(sid_idx_mapped + 1);
                sid_remainpv.resize(sid_idx_mapped + 1);
                sid_did_allocatepv.resize(sid_idx_mapped + 1);
            }
            
            // 第一次设置PV值
            sid_pv[sid_idx_mapped] = current_pv;
            sid_remainpv[sid_idx_mapped] = current_pv;
        }
        else
        {
            // 非首次出现，累加PV值
            sid_idx_mapped = sid_index[sid];
            int old_pv = sid_pv[sid_idx_mapped];
            sid_pv[sid_idx_mapped] += current_pv;
            sid_remainpv[sid_idx_mapped] += current_pv;
        }            
        
        std::vector<std::string> demand_list;
            split(strs[3], demand_list, ',');
            
            // 统计所有出现的需求（包括重复）
            total_demands_in_supply += demand_list.size();
            
            // 用于统计当前supply中有多少有效需求
            int current_supply_valid_demands = 0;

            for (const auto& demand_id : demand_list)
            {
                // 只处理在 demand 文件中存在的需求
                if (valid_demands.find(demand_id) != valid_demands.end()) {
                    current_supply_valid_demands++;
                    
                    if (did_index.find(demand_id) == did_index.end())
                    {
                        did_index[demand_id] = did_idx;
                        if (did_idx >= (int)index_did.size())  // 添加类型转换
                        {
                            index_did.resize(did_idx + 1);
                        }
                        index_did[did_idx] = demand_id;
                        did_idx++;
                    }

                    int demand_idx = did_index[demand_id];
                    sid_did[sid_idx_mapped].insert(demand_idx);

                    if (demand_idx >= (int)did_sid.size())  // 添加类型转换
                    {
                        did_sid.resize(demand_idx + 1);
                        // canallocate_did_sid.resize(demand_idx + 1);
                    }
                    did_sid[demand_idx].insert(sid_idx_mapped);
                    // canallocate_did_sid[demand_idx].insert(sid_idx_mapped);
                } else {
                    // 输出被过滤的无效需求（可选，用于调试）
                    // std::cout << "过滤无效需求: " << demand_id << " (在demand文件中不存在)" << std::endl;
                }
            }
            
            valid_demands_count += current_supply_valid_demands;
        }

        std::cout << "supply data size: " << sid_index.size() << std::endl;
        std::cout << "唯一有效需求ID数量: " << did_index.size() << std::endl;
        supply_file.close();
    }


    void sls_model::model_problem(std::string allocation_res_file, ModelMode mode, int cutoff_seconds)
    {
        std::cout << "supply num | demand num\n";
        std::cout << sid_pv.size() << " | " << did_amount.size() << std::endl;
        std::cout << std::endl;
        if(mode==Greedy)
        {
            auto start_time = std::chrono::high_resolution_clock::now();

            LocalSearch(allocation_res_file, mode);

            // local search 结束时间
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            std::cout << "Local search function execution time: " << duration << " ms" << std::endl;
            // 在线阶段：读取在线需求、FIFO分配、输出在线结果（包含离线+在线）
            std::string online_allocation_res_file = allocation_res_file + "_online.csv";
            mark_fixed_online_demand();
            FIFO_online(cutoff_seconds);
            LSout_online(online_allocation_res_file);

            // compute online metrics (gain/penalty) and print
            unsatisfied_total = 0;
            for (int rem : did_remain_amount) {
                unsatisfied_total += rem;
            }
            penalty = static_cast<double>(unsatisfied_total) * 1000;
            gain_metric = static_cast<double>(origin_gain_online) - penalty;
            std::cout << "Online metrics: origin_gain=" << origin_gain_online
                      << ", penalty=" << penalty
                      << ", gain=" << gain_metric
                      << ", unsatisfied_total=" << unsatisfied_total
                      << std::endl;
            std::cout << "OBJ: "<<gain_metric<<std::endl;
        }
    }

    
    std::vector<std::unordered_map<int, int>> sls_model::LocalSearch(std::string allocation_res_file, ModelMode mode)
    {
        // 标记在线专用需求，离线阶段跳过
        std::vector<char> is_online(did_sid.size(), 0);
        for (int did : last_online_list) {
            if (did >= 0 && did < (int)is_online.size()) {
                is_online[did] = 1;
            }
        }

        // 1. 计算每个 demand 能关联到的所有 supply 的 pv 总和
        std::vector<std::pair<int, int>> demand_total_pv; // (demand_idx, total_pv)
        for (size_t did_idx = 0; did_idx < did_sid.size(); ++did_idx) {
            if (is_online[did_idx]) continue; // 离线跳过在线需求
            int total_pv = 0;
            for (int sid_idx : did_sid[did_idx]) {
                total_pv += sid_pv[sid_idx];
            }
            demand_total_pv.emplace_back(did_idx, total_pv);
        }

        // 2. 按照 total_pv 升序排序 demand
        std::sort(demand_total_pv.begin(), demand_total_pv.end(), [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

        // 3. 为每个 demand 维护一个 alpha_j
        std::vector<double> alpha_j(did_sid.size(), 0.0); // 可根据需要初始化为0或其他


        // 4. 按照排序后的 demand 顺序依次分配
        for (const auto& [did_idx, total_pv] : demand_total_pv) {
            if (is_online[did_idx]) continue; // 再次防御性跳过
            double dj = did_remain_amount[did_idx];
            if (dj <= 0) continue;
            // 收集该 demand 的所有 supply 的 (sid_idx, r_i, s_i, t_i)
            struct SupplyInfo {
                int sid_idx;
                int r_i;
                int s_i;
                double t_i;
            };
            std::vector<SupplyInfo> supply_infos;
            for (int sid_idx : did_sid[did_idx]) {
                int r_i = sid_remainpv[sid_idx];
                int s_i = sid_pv[sid_idx];
                double t_i = s_i > 0 ? (double)r_i / s_i : 0.0;
                supply_infos.push_back({sid_idx, r_i, s_i, t_i});
            }
            // 按 t_i 升序排序
            std::sort(supply_infos.begin(), supply_infos.end(), [](const SupplyInfo& a, const SupplyInfo& b) {
                return a.t_i < b.t_i;
            });

            // 枚举分段，直接解 alpha_j
            double sum_r = 0.0;
            double sum_s = 0.0;
            for (const auto& info : supply_infos) sum_s += info.s_i;
            double alpha = 1.0;
            double left = 0.0;
            double right = supply_infos[0].t_i;

            for (size_t k = 0; k <= supply_infos.size(); ++k) {
                right = (k == supply_infos.size()) ? 1.0 : supply_infos[k].t_i;
                double candidate = (dj - sum_r) / sum_s;
                if (candidate >= left && candidate <= right && candidate <= 1.0 && candidate >= 0.0) {
                    alpha = candidate;
                    break;
                }
                if(k != supply_infos.size())sum_r += supply_infos[k].r_i;
                if(k != supply_infos.size())sum_s -= supply_infos[k].s_i;
                left=supply_infos[k].t_i;
            }
            alpha_j[did_idx] = alpha;

            // 分配并更新剩余 pv
            for (const auto& info : supply_infos) {
                int alloc = std::min(info.r_i, (int)std::round(info.s_i * alpha));
                sid_did_allocatepv[info.sid_idx][did_idx] += alloc;
                sid_remainpv[info.sid_idx] -= alloc;
                did_remain_amount[did_idx] -= alloc;
            }
        }

        

        std::string offline_allocation_res_file = allocation_res_file + "_offline.csv";
        LSout_offline(offline_allocation_res_file);
        return sid_did_allocatepv; // 返回整数索引版本的分配结果
    }
    void sls_model::mark_fixed_online_demand()
    {
        did_online_mask.clear();
        did_online_mask.resize(did_index.size(), 0);

        if (last_online_list.empty()) {
            std::cout << "Warning: no valid demand ID available for online allocation (demand file may be empty or all filtered)." << std::endl;
            return;
        }

        for (int did : last_online_list) {
            if (did >= 0 && did < (int)did_online_mask.size()) {
                did_online_mask[did] = 1;
            }
        }

        std::cout << "Online demands (tail of demand file, up to 50):" << std::endl;
        for (size_t i = 0; i < last_online_idlist.size(); ++i) {
            std::cout << "  " << last_online_idlist[i] << std::endl;
        }
    }

    void sls_model::FIFO_online(int cutoff_seconds)
    {
        using clock = std::chrono::steady_clock;
        auto time_start = clock::now();
        size_t processed_supply = 0;
        int timed_out = 0;

        for (int sid = 0; sid < (int)sid_remainpv.size(); ++sid) {
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

            // 按末尾顺序找到第一个仍可分配且关联该 supply 的在线需求，分配所有剩余 PV
            for (int did : last_online_list) {
                if (did >= (int)did_remain_amount.size()) continue;
                if (did_remain_amount[did] <= 0) continue;
                if (sid_did[sid].find(did) == sid_did[sid].end()) continue; // 不支持的需求跳过

                int allocate = remain;
                sid_did_allocatepv[sid][did] += allocate;
                origin_gain_online += allocate; // gain uses actual_transfer

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

    void sls_model::LSout_online(std::string online_res_file)
    {
        std::ofstream fout(online_res_file);
        if (!fout.is_open()) {
            std::cerr << "无法打开文件" << online_res_file << "进行写入。\n";
            return;
        }
        // 与参考 FIFO 保持一致：小写表头，输出离线+在线的完整分配集
        std::cout << "Online allocation results in file " << online_res_file << "\n";
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
    }

    void sls_model::LSout_offline(std::string offline_res_file)
    {
        // 检查哪些 demand 还没有被完全满足（只看出现在 user-supply 中的），同时统计出现在 supply 中的 demand 数量
        int total_demand_in_supply = 0;  
        std::vector<int> unfinished_demands;
        
        // 遍历所有 demand 索引
        for (size_t did_idx = 0; did_idx < did_remain_amount.size(); ++did_idx) {
            if (did_remain_amount[did_idx] > 0) {
                unfinished_demands.push_back(did_idx);
            }
        }

        // 输出 warning 信息
        if (unfinished_demands.empty()) {
            std::cout << "After offline allocatiion, all valid demands have been fully allocated." << std::endl;
        } else {
            std::cout << "Unfinished demand count: " << unfinished_demands.size() << std::endl;
            std::cout << "The following valid demands still have unallocated amounts:" << std::endl;
            
            int total_unallocated_amount = 0;  // 计算总的未分配量
            
            for (int did_idx : unfinished_demands) {
                int remaining_amount = did_remain_amount[did_idx];
                total_unallocated_amount += remaining_amount;
                
                std::string demand_id = index_did[did_idx];
                
                std::cout << "Demand ID: " << demand_id
                        << ", Remaining Amount: " << remaining_amount
                        << std::endl;
            }
            
            std::cout << "Total unallocated amount: " << total_unallocated_amount << std::endl;
        }

        // 输出分配结果到文件
        std::ofstream output_file(offline_res_file);
        if (!output_file.is_open()) {
            std::cerr << "无法打开文件" << offline_res_file << "进行写入。\n";
        } else {
            std::cout << "Offline allocation results in file " << offline_res_file << "\n";
            output_file << "SID,Demand,Allocated PV\n";
            for (size_t sid_idx = 0; sid_idx < sid_did_allocatepv.size(); ++sid_idx) {
                // 找到对应的字符串 sid 用于输出
                std::string sid = index_sid[sid_idx];
                
                
                for (const auto& [did_idx, allocated_pv] : sid_did_allocatepv[sid_idx]) {
                    // 找到对应的字符串 demand_id 用于输出
                    std::string demand_id= index_did[did_idx];
                    output_file << sid << "," 
                                << demand_id << "," 
                                << allocated_pv << "\n";
                    
                }
            }
            output_file.close();
        }
        std::cout << std::endl;
    }
    
};
