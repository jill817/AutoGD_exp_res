#include "solve.h"

#include "mode.h"

#include <iostream>
#include <fstream>
#include <chrono> // 添加头文件
#include <algorithm>
#include <random>
#include <queue>  // 添加队列头文件，用于BFS
#include <thread> // 添加线程支持
#include <atomic> // 添加原子操作支持
#include <pthread.h> // 添加pthread支持

/**
 * @brief Data Format Description
 * 1. integer dataset:  user id | supply id | cast times | demand_list (split by ,)
 * 2. demand dataset: demand id | demand amount
 * 3. var's name change
 */

namespace solver
{
    // 工作线程函数实现（类似 MIQP 的 WorkerSolve）
    void* WorkerParaGreedy(void* arg) {
        ParallelGreedyWorker* worker = (ParallelGreedyWorker*)arg;
        
        // 类似 MIQP 中的注释
        // cout << "worker[" << worker->thread_id << "] starting parallel greedy" << endl;
        
        // 对分配给当前线程的 sid_list 进行贪婪分配
        for (const auto& [sid_idx, heat] : worker->my_sid_list) {
            int remaining_pv = worker->main_solver->sid_remainpv[sid_idx];
            if (remaining_pv == 0) {
                continue;
            }
            
            const auto& demand_set = worker->main_solver->sid_did[sid_idx];
            
            // 构造当前 sid 下的 demand，并按全局排序排序
            std::vector<int> local_demands(demand_set.begin(), demand_set.end());
            std::sort(local_demands.begin(), local_demands.end(),
                      [&](int a, int b) {
                          return (*(worker->demand_rank))[a] < (*(worker->demand_rank))[b];
                      });
            
            // 分配 pv - 直接更新全局的 sid_did_allocatepv（无冲突）
            for (int did_idx : local_demands) {
                if (worker->my_did_remain_amount[did_idx] == 0) continue;
                int allocate_pv = std::min(remaining_pv, worker->my_did_remain_amount[did_idx]);
                worker->main_solver->sid_did_allocatepv[sid_idx][did_idx] += allocate_pv;
                remaining_pv -= allocate_pv;
                worker->my_did_remain_amount[did_idx] -= allocate_pv;
            }
            worker->main_solver->sid_remainpv[sid_idx] = remaining_pv;
        }
        
        worker->completed = true;
        return nullptr;
    }

    // 并行局部搜索工作线程函数
    void* WorkerParaLocalSearch(void* arg) {
        ParallelLocalSearchWorker* worker = static_cast<ParallelLocalSearchWorker*>(arg);

        sls_model* solver = worker->main_solver;
        
        // 开始计时：线程内初始化
        auto thread_init_start_time = std::chrono::high_resolution_clock::now();
        
        // 在线程内部并行构建本线程的供应索引集合，用于快速查找
        std::unordered_set<int> my_sid_set;
        for (const auto& [sid_idx, heat] : worker->my_sid_list) {
            my_sid_set.insert(sid_idx);
        }
        
        // 为每个demand构建本线程的供应集合
        for (size_t did_idx = 0; did_idx < solver->did_sid.size(); ++did_idx) {
            const std::unordered_set<int>& all_supplies = solver->did_sid[did_idx];
            for (int sid_idx : all_supplies) {
                if (my_sid_set.find(sid_idx) != my_sid_set.end()) {
                    worker->my_did_sid[did_idx].insert(sid_idx);
                }
            }
        }
        
        // 结束计时：线程内初始化
        auto thread_init_end_time = std::chrono::high_resolution_clock::now();
        auto thread_init_duration = std::chrono::duration_cast<std::chrono::milliseconds>(thread_init_end_time - thread_init_start_time).count();
        std::cout << "线程 " << worker->thread_id << " 内部初始化耗时: " << thread_init_duration << " ms" << std::endl;
        
        // 开始计时：while(true) 循环
        auto while_loop_start_time = std::chrono::high_resolution_clock::now();
        
        while(true) 
        {
            solver->local_greedy(worker);

            std::vector<int> my_unfinished_demands = solver->collect_local_unfinished_demands(worker);
            if (!my_unfinished_demands.empty()) {
                int local_move_pv = solver->local_optimization_parallel(worker, my_unfinished_demands);
                if (local_move_pv == 0) {
                    worker->my_feasible = false;
                    std::cout << "The thread "<<worker->thread_id<< " is infeasible" << std::endl;
                    break;
                } else {
                    if(local_move_pv < 0) {
                        worker->my_feasible = false;
                        std::cout << "Error: move_pv < 0" << std::endl;
                        break;
                    }
                }
            } else {
                worker->my_feasible = true;
                std::cout << "The thread "<<worker->thread_id<< " is feasible" << std::endl;
                break;
            }
        
        }
        
        // 结束计时：while(true) 循环
        auto while_loop_end_time = std::chrono::high_resolution_clock::now();
        auto while_loop_duration = std::chrono::duration_cast<std::chrono::milliseconds>(while_loop_end_time - while_loop_start_time).count();
        std::cout << "线程 " << worker->thread_id << " while循环耗时: " << while_loop_duration << " ms" << std::endl;
        
        // 开始计时：目标函数优化
        auto objective_opt_start_time = std::chrono::high_resolution_clock::now();
        
        // 4. 目标函数优化：根据线程自身的可行性状态决定策略
        if (worker->my_feasible) {
            std::cout << "线程 " << worker->thread_id << " 进入可行分支的目标优化阶段" << std::endl;
            // 4.1 本线程可行：直接进行目标优化
            for (double threshold = 0.1; threshold <= 0.9; threshold += 0.1) {
                solver->local_optimize_obj_parallel(worker, threshold);
                solver->local_greedy(worker);
            }
        } else {
            std::cout << "线程 " << worker->thread_id << " 进入不可行分支的目标优化阶段" << std::endl;
            // 4.2 本线程不可行：目标优化 + 缺量调整交替进行
            for (double threshold = 0.1; threshold <= 0.9; threshold += 0.1) {
                solver->local_optimize_obj_parallel(worker, threshold);
                solver->local_greedy(worker);
                
                // 收集未满足需求并进行局部优化
                std::vector<int> updated_unfinished_demands = solver->collect_local_unfinished_demands(worker);
                if (!updated_unfinished_demands.empty()) {
                    solver->local_optimization_parallel(worker, updated_unfinished_demands);
                } else {
                    worker->my_feasible = true;
                }
            }
        }
        
        // 结束计时：目标函数优化
        auto objective_opt_end_time = std::chrono::high_resolution_clock::now();
        auto objective_opt_duration = std::chrono::duration_cast<std::chrono::milliseconds>(objective_opt_end_time - objective_opt_start_time).count();
        std::cout << "线程 " << worker->thread_id << " 目标函数优化耗时: " << objective_opt_duration << " ms" << std::endl;
        
        return nullptr;
    }
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

    void sls_model::solve_problem(std::string allocation_res_file,ModelMode mode, std::vector<std::string> online_query_list)
    {
        std::cout << std::endl;
        read_supply_data(); // 内部已经包含两次过滤逻辑：先预加载demand，再过滤supply
        read_demand_data(); // 后读取 demand 文件
        read_heat_data();
        std::cout << std::endl;
        model_problem(allocation_res_file, mode, online_query_list);
    }

    void sls_model::read_demand_data()
    {
        did_amount.clear();
        did_amount.resize(did_index.size(), 0); 
        did_remain_amount.clear();
        did_remain_amount.resize(did_index.size(), 0);

        std::ifstream demand_file(demand_path);
        std::string record;

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
            }
            line_num++;
        }

        std::cout << "Valid demand count (both in supply and demand): " << valid_matched_demands << std::endl;
        
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

    /**
     * All ids or strs are used with converted ids
     */
    void sls_model::model_problem(std::string allocation_res_file,ModelMode mode, std::vector<std::string> online_query_list)
    {
        std::cout << "supply num | demand num\n";
        std::cout << sid_pv.size() << " | " << did_amount.size() << std::endl;
        std::cout << std::endl;
        if(mode==Greedy)
        {
            auto start_time = std::chrono::high_resolution_clock::now();

            LocalSearch(allocation_res_file, mode, online_query_list,start_time);

            // local search 结束时间
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            std::cout << "Local search function execution time: " << duration << " ms" << std::endl;
        }
    }

    
    std::vector<std::unordered_map<int, int>> sls_model::LocalSearch(std::string allocation_res_file, ModelMode mode, std::vector<std::string> online_query_list,std::chrono::high_resolution_clock::time_point start_time)
    {

        // 按 heat 从低到高排序 user-supply - 使用整数索引
        std::vector<std::pair<int, double>> sid_list; // (sid_idx, heat)
        for (size_t sid_idx = 0; sid_idx < sid_heat.size(); ++sid_idx) {
            sid_list.emplace_back(sid_idx, sid_heat[sid_idx]);
        }
        std::sort(sid_list.begin(), sid_list.end(),
                  [](const auto& a, const auto& b) {
                      return a.second < b.second; // 按热度系数升序排序
                  });

        // demand 排序
        std::vector<int> demand_count(did_sid.size(), 0);
        for (size_t did_idx = 0; did_idx < did_sid.size(); ++did_idx) {
            const std::unordered_set<int>& supply_set = did_sid[did_idx];
            for (int sid_idx : supply_set) 
            {
                if(sid_remainpv[sid_idx] > 0)
                {
                    demand_count[did_idx]++; // 只统计有剩余 pv 的 supply
                }
            }                
        }
        std::vector<std::pair<int, int>> sorted_demand_vec; // (demand_idx, count)
        for (size_t did_idx = 0; did_idx < demand_count.size(); ++did_idx) {
            if (demand_count[did_idx] > 0) { // 只考虑有出现的 demand
                sorted_demand_vec.emplace_back(did_idx, demand_count[did_idx]);
            }
        }
        std::sort(sorted_demand_vec.begin(), sorted_demand_vec.end(),
                [](const auto& a, const auto& b) {
                    return a.second < b.second; // 按出现频次升序排序
                });
        std::vector<int> demand_rank(did_amount.size(), -1); // 初始化为 -1，表示未排序
        for (size_t i = 0; i < sorted_demand_vec.size(); ++i) {
            int did_idx = sorted_demand_vec[i].first;
            demand_rank[did_idx] = i; // 设置排序等级
        }

        

        bool feasible = false;
        
        // 使用并行局部搜索调整到最小缺量并进行目标优化
        auto para_local_search_start_time = std::chrono::high_resolution_clock::now();
        feasible = para_local_search(sid_list, demand_rank, demand_count);
        auto para_local_search_end_time = std::chrono::high_resolution_clock::now();
        auto para_local_search_duration = std::chrono::duration_cast<std::chrono::milliseconds>(para_local_search_end_time - para_local_search_start_time).count();
        std::cout << "para_local_search求解耗时: " << para_local_search_duration << " ms" << std::endl;

        // 串行调整缺量
        if(!feasible)
        {
            auto serial_adjustment_start_time = std::chrono::high_resolution_clock::now();
            for(int t=0;;t++) // TODO:可以修改为cutoff_time以内
            {
                greedy(sid_list, demand_rank);
                // 收集未满足的需求 - 使用整数索引
                std::vector<int> unfinished_demands;
                for (size_t did_idx = 0; did_idx < did_remain_amount.size(); ++did_idx) {
                    if (did_remain_amount[did_idx] > 0 && demand_count[did_idx] > 0) {
                        unfinished_demands.push_back(did_idx);
                    }
                }
                
                // 如果有未满足的需求，进行局部优化
                if (!unfinished_demands.empty()) {
                    int move_pv = local_optimization(unfinished_demands);
                    if (move_pv == 0) {
                        std::cout << "The instance is infeasible" << std::endl;
                        break; // 如果没有可移动的 PV，退出循环
                    }
                    else
                    {
                        if(move_pv < 0) {
                            std::cout << "Error: move_pv < 0" << std::endl;
                            break;
                        }
                    }
                }
                else {
                    feasible = true;
                    std::cout << "The instance is feasible" << std::endl;
                    break; // 如果没有未满足的需求，退出循环
                }
                if(t%1000==0)
                {
                    auto now = std::chrono::high_resolution_clock::now();
                    int cutoff_time = 60;
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
                    if (elapsed > cutoff_time) {
                        std::cout << "超出时间限制，跳出循环" << std::endl;
                        break;
                    }
                }

            }
            auto serial_adjustment_end_time = std::chrono::high_resolution_clock::now();
            auto serial_adjustment_duration = std::chrono::duration_cast<std::chrono::milliseconds>(serial_adjustment_end_time - serial_adjustment_start_time).count();
            std::cout << "串行阶段调整缺量耗时: " << serial_adjustment_duration << " ms" << std::endl;
        }
        

        std::string offline_allocation_res_file = allocation_res_file + "_offline.csv";
        std::string online_allocation_res_file = allocation_res_file + "_online.csv";
        LSout_offline(offline_allocation_res_file);

        
        auto online_query_start_time = std::chrono::high_resolution_clock::now();
        online_query(online_query_list);
        auto online_query_end_time = std::chrono::high_resolution_clock::now();
        auto online_query_duration = std::chrono::duration_cast<std::chrono::milliseconds>(online_query_end_time - online_query_start_time).count();
        std::cout << "online_query求解耗时: " << online_query_duration << " ms" << std::endl;
        
        LSout_online(online_allocation_res_file);
        return sid_did_allocatepv; // 返回整数索引版本的分配结果
    }

    bool sls_model::para_local_search(std::vector<std::pair<int, double>>& sid_list, std::vector<int>& demand_rank, std::vector<int>& demand_count)
    {
        // 开始计时：工作线程初始化
        auto worker_init_start_time = std::chrono::high_resolution_clock::now();
        
        // 创建并初始化工作线程
        std::vector<ParallelLocalSearchWorker> workers(NUM_THREADS);
        initialize_parallel_workers(workers, sid_list, demand_rank, demand_count);
        
        // 使用类似MIQP的简洁方式：pthread + 顺序join
        std::vector<pthread_t> workerPtr(NUM_THREADS);
        parallel_running_ = true;
        
        // 结束计时：工作线程初始化
        auto worker_init_end_time = std::chrono::high_resolution_clock::now();
        auto worker_init_duration = std::chrono::duration_cast<std::chrono::milliseconds>(worker_init_end_time - worker_init_start_time).count();
        std::cout << "工作线程初始化耗时: " << worker_init_duration << " ms" << std::endl;
        
        std::cout << "Starting " << NUM_THREADS << " worker threads..." << std::endl;
        
        // 按MIQP的方式：顺序创建所有线程
        for (int i = 0; i < NUM_THREADS; i++) {
            int ret = pthread_create(&workerPtr[i], NULL, WorkerParaLocalSearch, (void*)&workers[i]);
            if (ret != 0) {
                std::cerr << "Error creating thread " << i << ": " << ret << std::endl;
                // 清理已创建的线程
                for (int j = 0; j < i; j++) {
                    pthread_cancel(workerPtr[j]);
                    pthread_join(workerPtr[j], NULL);
                }
                parallel_running_ = false;
                return false;
            }
        }
        
        std::cout << "All " << NUM_THREADS << " threads created successfully" << std::endl;
        
        // 按MIQP的方式：顺序等待所有线程完成
        for (int i = 0; i < NUM_THREADS; i++) {
            void* thread_result;
            int ret = pthread_join(workerPtr[i], &thread_result);
            if (ret != 0) {
                std::cerr << "Error joining thread " << i << ": " << ret << std::endl;
            } else {
                std::cout << "Thread " << i << " joined successfully" << std::endl;
            }
        }
        
        std::cout << "All " << NUM_THREADS << " threads completed and joined" << std::endl;
        
        // 开始计时：汇总工作线程结果
        auto aggregate_start_time = std::chrono::high_resolution_clock::now();
        
        // 最终汇总结果，并获取全局可行性状态
        bool global_feasible = aggregate_worker_results(workers);
        
        // 结束计时：汇总工作线程结果
        auto aggregate_end_time = std::chrono::high_resolution_clock::now();
        auto aggregate_duration = std::chrono::duration_cast<std::chrono::milliseconds>(aggregate_end_time - aggregate_start_time).count();
        std::cout << "汇总工作线程结果耗时: " << aggregate_duration << " ms" << std::endl;
        
        parallel_running_ = false;
        
        return global_feasible;
    }

    // 初始化并行工作线程
    void sls_model::initialize_parallel_workers(std::vector<ParallelLocalSearchWorker>& workers, 
                                               const std::vector<std::pair<int, double>>& sid_list,
                                               const std::vector<int>& demand_rank,
                                               const std::vector<int>& demand_count) {
        // 按round-robin方式分配供应
        for (size_t i = 0; i < sid_list.size(); ++i) {
            int thread_id = i % NUM_THREADS;
            workers[thread_id].my_sid_list.push_back(sid_list[i]);
        }
        
        // 为每个工作线程设置基本信息
        for (int i = 0; i < NUM_THREADS; ++i) {
            workers[i].thread_id = i;
            workers[i].main_solver = this;
            workers[i].demand_rank = &demand_rank;
            workers[i].demand_count = &demand_count;
            workers[i].should_continue = true;
            workers[i].my_feasible = false;
            workers[i].iteration_completed = false;
            
            // 初始化局部需求数组 - 每个线程处理全部需求的 1/NUM_THREADS
            workers[i].my_did_remain_amount.resize(did_remain_amount.size());
            workers[i].my_did_sid.resize(did_remain_amount.size());
            
            // 为所有需求分配 1/NUM_THREADS 的剩余量
            for (size_t did_idx = 0; did_idx < did_remain_amount.size(); ++did_idx) {
                int base_amount = did_remain_amount[did_idx] / NUM_THREADS;
                int remainder = did_remain_amount[did_idx] % NUM_THREADS;
                
                if (i < remainder) {
                    workers[i].my_did_remain_amount[did_idx] = base_amount + 1;
                } else {
                    workers[i].my_did_remain_amount[did_idx] = base_amount;
                }
            }
        }
    }

    // 检查全局可行性
    bool sls_model::check_global_feasibility(const std::vector<ParallelLocalSearchWorker>& workers) {
        // 汇总所有线程的需求剩余量
        std::vector<int> global_did_remain_amount(did_remain_amount.size(), 0);
        
        for (const auto& worker : workers) {
            for (size_t did_idx = 0; did_idx < global_did_remain_amount.size(); ++did_idx) {
                global_did_remain_amount[did_idx] += worker.my_did_remain_amount[did_idx];
            }
        }
        
        // 检查是否有未满足的需求
        for (size_t did_idx = 0; did_idx < global_did_remain_amount.size(); ++did_idx) {
            if (global_did_remain_amount[did_idx] > 0 && (*workers[0].demand_count)[did_idx] > 0) {
                return false;
            }
        }
        
        return true;
    }

    // 汇总工作线程结果
    bool sls_model::aggregate_worker_results(std::vector<ParallelLocalSearchWorker>& workers) {
        
        // 汇总所有线程的需求剩余量
        std::fill(did_remain_amount.begin(), did_remain_amount.end(), 0);
        for (const auto& worker : workers) {
            for (size_t did_idx = 0; did_idx < did_remain_amount.size(); ++did_idx) {
                did_remain_amount[did_idx] += worker.my_did_remain_amount[did_idx];
            }
        }
        
        // 更新全局可行性状态：只有当所有线程都可行时，全局才可行
        bool global_feasible = true;
        for (const auto& worker : workers) {
            if (!worker.my_feasible) {
                global_feasible = false;
                break;
            }
        }
        
        // 将全局可行性状态同步到所有工作线程
        for (auto& worker : workers) {
            worker.global_feasible = global_feasible;
        }
        
        return global_feasible;
    }

    int sls_model::optimize_obj(std::vector<std::pair<int, double>> sid_list, std::vector<int> demand_rank, double threshold)
    { 
        //调整使得目标函数更好
        std::vector<int> unfinished_demands;
        for (size_t sid_idx = 0; sid_idx < sid_list.size(); ++sid_idx) {
            int sid_idx_mapped = sid_list[sid_idx].first; // 使用整数索引
            double heat = sid_list[sid_idx].second;
            if (heat > threshold) {
                // 清零 sid_did_allocatepv[sid_idx_mapped] 的分配
                for (const auto& [did_idx, allocated_pv] : sid_did_allocatepv[sid_idx_mapped]) {
                    sid_remainpv[sid_idx_mapped] += allocated_pv; // 增加剩余 pv
                    did_remain_amount[did_idx] += allocated_pv;   // 增加对应 demand 的剩余需求量
                    if (std::find(unfinished_demands.begin(), unfinished_demands.end(), did_idx) == unfinished_demands.end()) {
                        unfinished_demands.push_back(did_idx); // 仅当 did_idx 不在 unfinished_demands 中时添加
                    }
                }
                sid_did_allocatepv[sid_idx_mapped].clear(); // 清空分配记录
            }
        }
        std::cout<< "In optimize_obj, threshold: "<< threshold<< std::endl;
        std::cout<< "In optimize_obj, unfinished_demands.size(): "<< unfinished_demands.size()<< std::endl;
        int max_move_pv=move_pv(unfinished_demands,threshold);
        std::cout << "In optimize_obj, total Max moved PV: " << max_move_pv << std::endl;
        return max_move_pv;
    }

    void sls_model::greedy(std::vector<std::pair<int, double>> sid_list, std::vector<int> demand_rank)
    { 
        // 开始分配 pv
        for (const auto& [sid_idx, heat] : sid_list) 
        {
            int remaining_pv = sid_remainpv[sid_idx]; // 当前 sid 的剩余 pv
            if (remaining_pv == 0)
                {
                    continue;
                }
            const auto& demand_set = sid_did[sid_idx]; // 当前 sid 关联的 demand 集合

            // 构造当前 sid 下的 demand，并按全局排序排序
            std::vector<int> local_demands(demand_set.begin(), demand_set.end());
            std::sort(local_demands.begin(), local_demands.end(),
                      [&](int a, int b) {
                          return demand_rank[a] < demand_rank[b]; // 按照全局排序分配
                      });

            // 分配 pv
            for (int did_idx : local_demands) {
                if (did_remain_amount[did_idx] == 0) continue;
                int allocate_pv = std::min(remaining_pv, did_remain_amount[did_idx]);
                sid_did_allocatepv[sid_idx][did_idx] += allocate_pv;
                remaining_pv -= allocate_pv;
                did_remain_amount[did_idx] -= allocate_pv;
            }
            sid_remainpv[sid_idx] = remaining_pv;
        }
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
    void sls_model::LSout_online(std::string online_res_file)
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
            std::cout << "After online allocatiion, all valid demands have been fully allocated." << std::endl;
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
        std::ofstream output_file(online_res_file);
        if (!output_file.is_open()) {
            std::cerr << "无法打开文件" << online_res_file << "进行写入。\n";
        } else {
            std::cout << "Online allocation results in file " << online_res_file << "\n";
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
    }

    int sls_model::online_query(std::vector<std::string> demand_ids)
    { 
        auto start_time = std::chrono::high_resolution_clock::now();

        optimize_online(demand_ids);
        for (const auto& demand_id : demand_ids) {
            int remaining_pv = query_remaining_pv_for_demand(demand_id);
            std::cout << "Online query, max pv for Demand " << demand_id << ": " << remaining_pv << std::endl;
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        std::cout << "Online_query execution time: " << duration << " ms" << std::endl;

        return 0;
    }
    int sls_model::optimize_online(std::vector<std::string> demand_ids)
    { 
        // 在线分配
        std::vector<int> unfinished_demands;
        for (const auto& demand_id : demand_ids) {
            if (did_index.find(demand_id) != did_index.end()) {
                unfinished_demands.push_back(did_index[demand_id]);
            } else {
                std::cerr << "Warning: Demand ID " << demand_id << " not found in demand index." << std::endl;
            }
        }
        
        int max_move_pv = move_pv(unfinished_demands, 1);
        std::cout << "In optimize_online, total Max moved PV: " << max_move_pv << std::endl;
        return max_move_pv;
    }
    int sls_model::query_remaining_pv_for_demand(std::string demand_id)
    { 
        // 记录开始时间
        auto start_time = std::chrono::high_resolution_clock::now();

        // 检查 demand_id 是否存在于索引映射中
        if (did_index.find(demand_id) == did_index.end()) {
            std::cerr << "Error: Demand ID " << demand_id << " not found in demand index." << std::endl;
            return 0;
        }
        
        // 获取 demand 的整数索引
        int demand_idx = did_index[demand_id];
        
        // 检查索引是否有效
        if (demand_idx < 0 || demand_idx >= did_sid.size()) {
            std::cerr << "Error: Invalid demand index " << demand_idx << " for demand ID " << demand_id << std::endl;
            return 0;
        }
        
        // 获取该 demand 关联的所有 supply 索引
        const std::unordered_set<int>& associated_supplies = did_sid[demand_idx];
        
        // 计算所有关联 supply 的剩余 PV 总和
        int total_remaining_pv = 0;
        for (int supply_idx : associated_supplies) {
            // 检查 supply 索引是否有效
            if (supply_idx >= 0 && supply_idx < sid_remainpv.size()) {
                total_remaining_pv += sid_remainpv[supply_idx];
            } else {
                std::cerr << "Error: Invalid supply index " << supply_idx << " for demand " << demand_id << std::endl;
            }
        }
        total_remaining_pv = total_remaining_pv + did_amount[demand_idx] - did_remain_amount[demand_idx];
        // std::cout << "satisfied pv for Demand " << demand_id << " is " << did_amount[demand_idx] - did_remain_amount[demand_idx] << std::endl;

        // auto end_time = std::chrono::high_resolution_clock::now();
        // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        // std::cout << "query_remaining_pv_for_demand execution time (online process): " << duration << " ms" << std::endl;

        return total_remaining_pv;
    }


    int sls_model::local_optimization(
        std::vector<int>& unfinished_demands)
    {
        int max_move_pv = move_pv(unfinished_demands,1);
        return max_move_pv;
    }

    int sls_model::move_pv(std::vector<int>& unfinished_demands, double threshold)
    { 
        std::unordered_set<int> unfinished_needed_supply; 
        for (int demand_idx : unfinished_demands) {
            const std::unordered_set<int>& supply_set = did_sid[demand_idx];
            unfinished_needed_supply.insert(supply_set.begin(), supply_set.end());
        }

        std::vector<int> demand_allocatepv_from_neededsupply(did_amount.size(), 0); 
        for (int supply_id : unfinished_needed_supply) {
            const auto& allocation_map = sid_did_allocatepv[supply_id]; 
            for (const auto& [demand_id, allocated_pv] : allocation_map) {
                demand_allocatepv_from_neededsupply[demand_id] += allocated_pv; 
            }
        }

        // 遍历所有已完成的需求，把有余量的、热度在阈值以下的 supply 分出来，直到没有可分的或者能完全代替用掉的被需要的 supply
        int max_move_pv = 0;
        for (size_t demand_idx = 0; demand_idx <  did_sid.size(); ++demand_idx) 
        {
            if (std::find(unfinished_demands.begin(), unfinished_demands.end(), demand_idx) != unfinished_demands.end()) {
                continue;
            }
            int need_pv = demand_allocatepv_from_neededsupply[demand_idx];
            if (need_pv <= 0) {
                if (need_pv < 0) {
                    std::cout << "error: need_pv < 0" << std::endl;
                }
                continue;
            }
            std::unordered_set<int> canallocate_sid =  did_sid[demand_idx];


            int moved_pv = 0;
            // 把有余量的 supply 分给当前 demand，记录一共分给了多少
            for (int supply_idx : canallocate_sid) { 
                if (unfinished_needed_supply.find(supply_idx) != unfinished_needed_supply.end()) {
                    continue; 
                }
                if (sid_heat[supply_idx] >= threshold)continue;
                int tmp_moved_pv = std::min(sid_remainpv[supply_idx], need_pv);
                moved_pv += tmp_moved_pv;
                need_pv -= tmp_moved_pv;

                sid_remainpv[supply_idx] -= tmp_moved_pv;
                sid_did_allocatepv[supply_idx][demand_idx] += tmp_moved_pv;
                demand_allocatepv_from_neededsupply[demand_idx] -= tmp_moved_pv; // 好像没意义但还是先写上

                
                if (need_pv <= 0) {
                    if (need_pv < 0) {
                        std::cerr << "Error: need_pv is negative after allocation." << std::endl;
                    }
                    break;
                }
            }
            max_move_pv += moved_pv;

            int true_move_pv = moved_pv;
            // 把被需要的 supply 分给当前 demand 的 pv 退回去
            for (int supply_idx : unfinished_needed_supply) 
            {
                if (sid_did_allocatepv[supply_idx].find(demand_idx) == sid_did_allocatepv[supply_idx].end()) {
                    continue; 
                }

                int tmp_moved_pv = std::min(sid_did_allocatepv[supply_idx][demand_idx],true_move_pv) ;
                true_move_pv -= tmp_moved_pv;

                sid_remainpv[supply_idx] += tmp_moved_pv;
                sid_did_allocatepv[supply_idx][demand_idx] -= tmp_moved_pv;

                if (true_move_pv <= 0) {
                    if (true_move_pv < 0) {
                        std::cerr << "Error: need_pv is negative after allocation." << std::endl;
                    }
                    break;
                }
            }
        }
        return max_move_pv; // 返回最大移动的 PV 数量
    }

    // === 并行局部搜索核心函数实现 ===
    
    void sls_model::local_greedy(ParallelLocalSearchWorker* worker) {
        // 基于串行greedy函数的并行版本
        for (const auto& [sid_idx, heat] : worker->my_sid_list) {
            int remaining_pv = sid_remainpv[sid_idx];
            if (remaining_pv == 0) {
                continue;
            }
            
            const auto& demand_set = sid_did[sid_idx];
            
            // 构造当前 sid 下的 demand，并按全局排序排序
            std::vector<int> local_demands(demand_set.begin(), demand_set.end());
            std::sort(local_demands.begin(), local_demands.end(),
                      [&](int a, int b) {
                          return (*(worker->demand_rank))[a] < (*(worker->demand_rank))[b];
                      });
            
            // 分配 pv - 无需锁，因为每个线程的supply不重合，demand是按比例分割的
            for (int did_idx : local_demands) {
                if (worker->my_did_remain_amount[did_idx] == 0) continue;
                int allocate_pv = std::min(remaining_pv, worker->my_did_remain_amount[did_idx]);
                
                // 直接更新，无需锁
                sid_did_allocatepv[sid_idx][did_idx] += allocate_pv;
                remaining_pv -= allocate_pv;
                worker->my_did_remain_amount[did_idx] -= allocate_pv;
            }
            
            // 直接更新供应剩余PV，无需锁
            sid_remainpv[sid_idx] = remaining_pv;
        }
    }
    
    std::vector<int> sls_model::collect_local_unfinished_demands(ParallelLocalSearchWorker* worker) {
        std::vector<int> unfinished_demands;
        
        // 收集本线程负责的所有未完成需求
        for (size_t did_idx = 0; did_idx < worker->my_did_remain_amount.size(); ++did_idx) {
            if (worker->my_did_remain_amount[did_idx] > 0) {
                unfinished_demands.push_back(did_idx);
            }
        }
        
        return unfinished_demands;
    }
    
    int sls_model::local_optimization_parallel(ParallelLocalSearchWorker* worker, std::vector<int>& my_unfinished_demands) {
        // 基于串行local_optimization的并行版本
        std::unordered_set<int> unfinished_needed_supply;
        for (int demand_idx : my_unfinished_demands) {
            // 使用预计算的 my_did_sid，只包含本线程的supply
            const std::unordered_set<int>& supply_set = worker->my_did_sid[demand_idx];
            unfinished_needed_supply.insert(supply_set.begin(), supply_set.end());
        }
        return move_pv_parallel(worker, my_unfinished_demands, unfinished_needed_supply, 1.0);
    }
    
    int sls_model::move_pv_parallel(ParallelLocalSearchWorker* worker, std::vector<int>& unfinished_demands, 
                                   std::unordered_set<int>& unfinished_needed_supply, double threshold) 
                                   {
        std::vector<int> demand_allocatepv_from_neededsupply(did_amount.size(), 0);
        // 无需锁：unfinished_needed_supply 中的所有supply都来自当前线程的my_did_sid，不存在跨线程竞争
        for (int supply_id : unfinished_needed_supply) {
            const auto& allocation_map = sid_did_allocatepv[supply_id];
            for (const auto& [demand_id, allocated_pv] : allocation_map) {
                demand_allocatepv_from_neededsupply[demand_id] += allocated_pv;
            }
        }

        int max_move_pv = 0;
        for (size_t demand_idx = 0; demand_idx < worker->my_did_remain_amount.size(); ++demand_idx) {
            if (std::find(unfinished_demands.begin(), unfinished_demands.end(), demand_idx) != unfinished_demands.end()) {
                continue;
            }
            
            int need_pv = demand_allocatepv_from_neededsupply[demand_idx];
            if (need_pv <= 0) continue;
            
            const std::unordered_set<int>& canallocate_sid = worker->my_did_sid[demand_idx];
            int moved_pv = 0;
            
            // 把有余量的 supply 分给当前 demand
            for (int supply_idx : canallocate_sid) {
                if (unfinished_needed_supply.find(supply_idx) != unfinished_needed_supply.end()) {
                    continue;
                }
                if (sid_heat[supply_idx] >= threshold) continue;
                
                // 无需锁：supply_idx来自当前线程的my_did_sid，不存在跨线程竞争
                int tmp_moved_pv = std::min(sid_remainpv[supply_idx], need_pv);
                moved_pv += tmp_moved_pv;
                need_pv -= tmp_moved_pv;

                sid_remainpv[supply_idx] -= tmp_moved_pv;
                sid_did_allocatepv[supply_idx][demand_idx] += tmp_moved_pv;
                
                if (need_pv <= 0) break;
            }
            max_move_pv += moved_pv;

            int true_move_pv = moved_pv;
            // 把被需要的 supply 分给当前 demand 的 pv 退回去
            for (int supply_idx : unfinished_needed_supply) {
                // 无需锁：supply_idx来自当前线程的unfinished_needed_supply，不存在跨线程竞争
                if (sid_did_allocatepv[supply_idx].find(demand_idx) == sid_did_allocatepv[supply_idx].end()) {
                    continue;
                }

                int tmp_moved_pv = std::min(sid_did_allocatepv[supply_idx][demand_idx], true_move_pv);
                true_move_pv -= tmp_moved_pv;

                sid_remainpv[supply_idx] += tmp_moved_pv;
                sid_did_allocatepv[supply_idx][demand_idx] -= tmp_moved_pv;

                if (true_move_pv <= 0) break;
            }
        }
        return max_move_pv;
    }
    
    void sls_model::local_optimize_obj_parallel(ParallelLocalSearchWorker* worker, double threshold) {
        // 基于串行optimize_obj函数的并行版本
        std::vector<int> unfinished_demands;
        
        // 清空热度高于阈值的供应分配 - 无需锁，因为每个线程只操作自己的supply
        for (const auto& [sid_idx, heat] : worker->my_sid_list) {
            if (heat > threshold) {
                for (const auto& [did_idx, allocated_pv] : sid_did_allocatepv[sid_idx]) {
                    sid_remainpv[sid_idx] += allocated_pv;
                    worker->my_did_remain_amount[did_idx] += allocated_pv;
                    if (std::find(unfinished_demands.begin(), unfinished_demands.end(), did_idx) == unfinished_demands.end()) {
                        unfinished_demands.push_back(did_idx);
                    }
                }
                sid_did_allocatepv[sid_idx].clear();
            }
        }

        // for (size_t sid_idx = 0; sid_idx < sid_list.size(); ++sid_idx) {
        //     int sid_idx_mapped = sid_list[sid_idx].first; // 使用整数索引
        //     double heat = sid_list[sid_idx].second;
        //     if (heat > threshold) {
        //         // 清零 sid_did_allocatepv[sid_idx_mapped] 的分配
        //         for (const auto& [did_idx, allocated_pv] : sid_did_allocatepv[sid_idx_mapped]) {
        //             sid_remainpv[sid_idx_mapped] += allocated_pv; // 增加剩余 pv
        //             did_remain_amount[did_idx] += allocated_pv;   // 增加对应 demand 的剩余需求量
        //             if (std::find(unfinished_demands.begin(), unfinished_demands.end(), did_idx) == unfinished_demands.end()) {
        //                 unfinished_demands.push_back(did_idx); // 仅当 did_idx 不在 unfinished_demands 中时添加
        //             }
        //         }
        //         sid_did_allocatepv[sid_idx_mapped].clear(); // 清空分配记录
        //     }
        // }
        
        if (!unfinished_demands.empty()) {
            std::unordered_set<int> unfinished_needed_supply;
            for (int demand_idx : unfinished_demands) {
                // 使用预计算的 my_did_sid，只包含本线程的supply
                const std::unordered_set<int>& supply_set = worker->my_did_sid[demand_idx];
                unfinished_needed_supply.insert(supply_set.begin(), supply_set.end());
            }
            move_pv_parallel(worker, unfinished_demands, unfinished_needed_supply, threshold);
        }

        // std::cout<< "In optimize_obj, threshold: "<< threshold<< std::endl;
        // std::cout<< "In optimize_obj, unfinished_demands.size(): "<< unfinished_demands.size()<< std::endl;
        // int max_move_pv=move_pv(unfinished_demands,threshold);
        // std::cout << "In optimize_obj, total Max moved PV: " << max_move_pv << std::endl;
        // return max_move_pv;
    }

    // 连通分支检测方法实现
    std::vector<sls_model::ComponentInfo> sls_model::find_components() {
        std::vector<ComponentInfo> all_components;
        std::vector<ComponentInfo> valid_components;
        std::vector<bool> supply_visited(sid_did.size(), false);
        std::vector<bool> demand_visited(did_sid.size(), false);
        
        // 从每个未访问的supply节点开始BFS
        for (int sid_idx = 0; sid_idx < sid_did.size(); ++sid_idx) {
            if (supply_visited[sid_idx]) continue;
            
            // 开始新的连通分支
            ComponentInfo component;
            std::queue<int> supply_queue, demand_queue;
            
            // 从当前supply开始
            supply_queue.push(sid_idx);
            supply_visited[sid_idx] = true;
            
            // BFS遍历
            while (!supply_queue.empty() || !demand_queue.empty()) {
                // 处理supply队列
                while (!supply_queue.empty()) {
                    int current_sid = supply_queue.front();
                    supply_queue.pop();
                    component.supply_indices.push_back(current_sid);
                    
                    // 访问所有相邻的demand
                    for (int did_idx : sid_did[current_sid]) {
                        if (!demand_visited[did_idx]) {
                            demand_visited[did_idx] = true;
                            demand_queue.push(did_idx);
                        }
                    }
                }
                
                // 处理demand队列
                while (!demand_queue.empty()) {
                    int current_did = demand_queue.front();
                    demand_queue.pop();
                    component.demand_indices.push_back(current_did);
                    
                    // 访问所有相邻的supply
                    for (int sid_idx_neighbor : did_sid[current_did]) {
                        if (!supply_visited[sid_idx_neighbor]) {
                            supply_visited[sid_idx_neighbor] = true;
                            supply_queue.push(sid_idx_neighbor);
                        }
                    }
                }
            }
            
            if (!component.supply_indices.empty()) {
                all_components.push_back(component);
                
                // 只有多个节点的连通分支才加入有效分支
                int total_nodes = component.supply_indices.size() + component.demand_indices.size();
                if (total_nodes > 1) {
                    valid_components.push_back(component);
                }
            }
        }
        
        int single_node_count = all_components.size() - valid_components.size();
        std::cout << "检测到 " << all_components.size() << " 个连通分支，其中 " 
                  << single_node_count << " 个只有一个节点，不处理；需要处理 " 
                  << valid_components.size() << " 个连通分支" << std::endl;
        
        return valid_components;
    }

    void sls_model::solve_single_component(const ComponentInfo& component, const std::string& output_file, 
                                          ModelMode mode, const std::vector<std::string>& online_query_list,
                                          std::chrono::high_resolution_clock::time_point start_time) {
        // 开始计时：分支数据准备
        auto data_prepare_start_time = std::chrono::high_resolution_clock::now();
        
        // 保存原始状态
        auto original_sid_remainpv = sid_remainpv;
        auto original_sid_did_allocatepv = sid_did_allocatepv;
        auto original_did_remain_amount = did_remain_amount;
        
        // 创建快速查找集合，避免重复的线性搜索
        std::unordered_set<int> supply_set(component.supply_indices.begin(), component.supply_indices.end());
        std::unordered_set<int> demand_set(component.demand_indices.begin(), component.demand_indices.end());
        
        // 重置分配状态，只针对当前分支
        for (int sid_idx : component.supply_indices) {
            sid_remainpv[sid_idx] = sid_pv[sid_idx];  // 重置为原始容量
            sid_did_allocatepv[sid_idx].clear();
        }
        
        for (int did_idx : component.demand_indices) {
            did_remain_amount[did_idx] = did_amount[did_idx];  // 重置为原始需求
        }
        
        // 对其他不在该分支的元素置零（暂时屏蔽）- 使用unordered_set进行O(1)查找
        for (int sid_idx = 0; sid_idx < sid_remainpv.size(); ++sid_idx) {
            if (supply_set.find(sid_idx) == supply_set.end()) {
                sid_remainpv[sid_idx] = 0;
            }
        }
        
        for (int did_idx = 0; did_idx < did_remain_amount.size(); ++did_idx) {
            if (demand_set.find(did_idx) == demand_set.end()) {
                did_remain_amount[did_idx] = 0;
            }
        }
        
        // 结束计时：分支数据准备
        auto data_prepare_end_time = std::chrono::high_resolution_clock::now();
        auto data_prepare_duration = std::chrono::duration_cast<std::chrono::milliseconds>(data_prepare_end_time - data_prepare_start_time).count();
        
        std::cout << "求解分支: " << component.supply_indices.size() << " 个供应, " 
                  << component.demand_indices.size() << " 个需求" << std::endl;
        std::cout << "分支数据准备耗时: " << data_prepare_duration << " ms" << std::endl;
        
        // 开始计时：LocalSearch求解
        auto localsearch_start_time = std::chrono::high_resolution_clock::now();
        
        LocalSearch(output_file, mode, online_query_list,start_time);
        
        // 结束计时：LocalSearch求解
        auto localsearch_end_time = std::chrono::high_resolution_clock::now();
        auto localsearch_duration = std::chrono::duration_cast<std::chrono::milliseconds>(localsearch_end_time - localsearch_start_time).count();
        std::cout << "分支LocalSearch求解耗时: " << localsearch_duration << " ms" << std::endl;
        
        // 恢复原始状态（如果需要继续处理其他分支）
        sid_remainpv = original_sid_remainpv;
        sid_did_allocatepv = original_sid_did_allocatepv;
        did_remain_amount = original_did_remain_amount;
    }

    void sls_model::solve_with_components(std::string output_base, ModelMode mode, std::vector<std::string> online_query_list) {
        // 开始计时：整个solve_with_components函数
        auto total_start_time = std::chrono::high_resolution_clock::now();
        
        std::cout << std::endl << "=== 开始solve_with_components求解 ===" << std::endl;
        
        // 1. 读取并构建全局数据结构（使用两次过滤法）
        std::cout << std::endl << "步骤1: 读取并构建全局数据结构" << std::endl;
        auto data_reading_start_time = std::chrono::high_resolution_clock::now();
        
        std::cout << "读取供应数据..." << std::endl;
        auto supply_start_time = std::chrono::high_resolution_clock::now();
        read_supply_data(); // 内部已经包含两次过滤逻辑：先预加载demand，再过滤supply
        auto supply_end_time = std::chrono::high_resolution_clock::now();
        auto supply_duration = std::chrono::duration_cast<std::chrono::milliseconds>(supply_end_time - supply_start_time).count();
        std::cout << "供应数据读取耗时: " << supply_duration << " ms" << std::endl;
        
        std::cout << "读取需求数据..." << std::endl;
        auto demand_start_time = std::chrono::high_resolution_clock::now();
        read_demand_data(); // 后读取 demand 文件
        auto demand_end_time = std::chrono::high_resolution_clock::now();
        auto demand_duration = std::chrono::duration_cast<std::chrono::milliseconds>(demand_end_time - demand_start_time).count();
        std::cout << "需求数据读取耗时: " << demand_duration << " ms" << std::endl;
        
        std::cout << "读取热度数据..." << std::endl;
        auto heat_start_time = std::chrono::high_resolution_clock::now();
        read_heat_data();
        auto heat_end_time = std::chrono::high_resolution_clock::now();
        auto heat_duration = std::chrono::duration_cast<std::chrono::milliseconds>(heat_end_time - heat_start_time).count();
        std::cout << "热度数据读取耗时: " << heat_duration << " ms" << std::endl;
        
        auto data_reading_end_time = std::chrono::high_resolution_clock::now();
        auto data_reading_duration = std::chrono::duration_cast<std::chrono::milliseconds>(data_reading_end_time - data_reading_start_time).count();
        std::cout << "数据读取总耗时: " << data_reading_duration << " ms" << std::endl;
        std::cout << std::endl;
        
        std::cout << "全局数据统计: " << sid_pv.size() << " 个供应, " << did_amount.size() << " 个需求" << std::endl;
        
        // 2. 检测连通分支（现在基于干净的数据）
        std::cout << std::endl << "步骤2: 检测连通分支" << std::endl;
        auto component_detection_start_time = std::chrono::high_resolution_clock::now();
        auto components = find_components();
        auto component_detection_end_time = std::chrono::high_resolution_clock::now();
        auto component_detection_duration = std::chrono::duration_cast<std::chrono::milliseconds>(component_detection_end_time - component_detection_start_time).count();
        std::cout << "连通分支检测耗时: " << component_detection_duration << " ms" << std::endl;
        
        if (components.empty()) {
            std::cout << "未检测到需要处理的连通分支，使用原始求解方法" << std::endl;
            auto original_solve_start_time = std::chrono::high_resolution_clock::now();
            model_problem(output_base, mode, online_query_list);
            auto original_solve_end_time = std::chrono::high_resolution_clock::now();
            auto original_solve_duration = std::chrono::duration_cast<std::chrono::milliseconds>(original_solve_end_time - original_solve_start_time).count();
            std::cout << "原始求解方法耗时: " << original_solve_duration << " ms" << std::endl;
            
            auto total_end_time = std::chrono::high_resolution_clock::now();
            auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end_time - total_start_time).count();
            std::cout << std::endl << "=== solve_with_components总耗时: " << total_duration << " ms ===" << std::endl;
            return;
        }
        
        // 3. 为每个连通分支单独求解
        std::cout << std::endl << "步骤3: 分别求解各个连通分支" << std::endl;
        auto components_solving_start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<long long> component_durations(components.size());
        long long total_component_time = 0;
        
        for (size_t i = 0; i < components.size(); ++i) {
            std::cout << std::endl << "=== 处理连通分支 " << (i + 1) << "/" << components.size() << " ===" << std::endl;
            auto single_component_start_time = std::chrono::high_resolution_clock::now();
            
            std::string component_output = output_base + "_component_" + std::to_string(i + 1);
            solve_single_component(components[i], component_output, mode, online_query_list,single_component_start_time);
            
            auto single_component_end_time = std::chrono::high_resolution_clock::now();
            component_durations[i] = std::chrono::duration_cast<std::chrono::milliseconds>(single_component_end_time - single_component_start_time).count();
            total_component_time += component_durations[i];
            
            std::cout << "分支 " << (i + 1) << " 求解耗时: " << component_durations[i] << " ms" << std::endl;
            std::cout << "=== 分支 " << (i + 1) << " 处理完成 ===" << std::endl;
        }
        
        auto components_solving_end_time = std::chrono::high_resolution_clock::now();
        auto components_solving_duration = std::chrono::duration_cast<std::chrono::milliseconds>(components_solving_end_time - components_solving_start_time).count();
        
        // 输出各分支耗时汇总
        std::cout << std::endl << "=== 各连通分支求解时间统计 ===" << std::endl;
        for (size_t i = 0; i < components.size(); ++i) {
            std::cout << "分支 " << (i + 1) << ": " << component_durations[i] << " ms" 
                      << " (供应数: " << components[i].supply_indices.size() 
                      << ", 需求数: " << components[i].demand_indices.size() << ")" << std::endl;
        }
        std::cout << "所有分支求解总耗时: " << components_solving_duration << " ms" << std::endl;
        std::cout << "分支求解累计耗时: " << total_component_time << " ms" << std::endl;
        
        // 输出整个函数的时间统计
        auto total_end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end_time - total_start_time).count();
        
        std::cout << std::endl << "=== solve_with_components完整时间统计 ===" << std::endl;
        std::cout << "1. 数据读取阶段: " << data_reading_duration << " ms" << std::endl;
        std::cout << "   - 供应数据读取: " << supply_duration << " ms" << std::endl;
        std::cout << "   - 需求数据读取: " << demand_duration << " ms" << std::endl;
        std::cout << "   - 热度数据读取: " << heat_duration << " ms" << std::endl;
        std::cout << "2. 连通分支检测: " << component_detection_duration << " ms" << std::endl;
        std::cout << "3. 分支求解阶段: " << components_solving_duration << " ms" << std::endl;
        std::cout << "总计: " << total_duration << " ms" << std::endl;
        std::cout << "=== solve_with_components求解完成 ===" << std::endl;
    }


};
