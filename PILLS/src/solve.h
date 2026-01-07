#pragma once


#include "mode.h"
#include <unordered_set>
#include <thread>
#include <boost/thread.hpp>
#include <boost/thread/thread.hpp>
#include <mutex>
#include <atomic>
#include <vector>
#include "util.h"

namespace solver {
    
    // 前声明
    class sls_model;
    
    // 并行工作线程的参数结构体（类似 MIQP 的 qp_solver）
    struct ParallelGreedyWorker {
        int thread_id;
        std::vector<std::pair<int, double>> my_sid_list;
        std::vector<int> my_did_remain_amount;
        std::vector<std::unordered_map<int, int>> my_sid_did_allocatepv;
        
        // 指向主求解器的指针（类似 MIQP 的 parallel_solver）
        sls_model* main_solver;
        
        // 只读全局数据的引用
        const std::vector<int>* demand_rank;
        std::atomic<bool> completed{false};
    };
    
    // 并行局部搜索工作线程结构体
    struct ParallelLocalSearchWorker {
        int thread_id;
        sls_model* main_solver;
        
        // 分配的数据子集
        std::vector<std::pair<int, double>> my_sid_list;
        std::vector<int> my_did_remain_amount;  // 每个线程负责全部需求的 1/NUM_THREADS
        std::vector<std::unordered_set<int>> my_did_sid;  // 每个demand对应的本线程supply集合
        
        // 状态信息
        bool my_feasible;
        bool global_feasible;  // 全局可行性状态
        
        // 共享数据引用
        const std::vector<int>* demand_rank;
        const std::vector<int>* demand_count;
        
        // 同步控制
        std::atomic<bool> phase_completed{false};        // 阶段完成标志
        std::atomic<bool> iteration_completed{false};    // 迭代完成标志
        std::atomic<bool> should_continue{true};         // 是否继续主循环
        std::atomic<bool> should_continue_to_obj_phase{true}; // 是否继续到目标优化阶段
    };
    
    // 工作线程函数声明（类似 MIQP 的 WorkerSolve）
    void* WorkerParaGreedy(void* arg);
    void* WorkerParaLocalSearch(void* arg);
    
    class sls_model {
        // 友元函数声明，允许工作线程访问私有成员
        friend void* WorkerParaGreedy(void* arg);
        friend void* WorkerParaLocalSearch(void* arg);
        
    private:

        std::string format_supply_path, demand_path, heat_path;

        std::vector<int> did_amount; // demand id --> amount (使用索引代替字符串键)
        std::vector<int> sid_pv; // supply id --> pv (使用索引代替字符串键)
        std::vector<std::unordered_set<int>> sid_did; // supply id --> demand id set (使用索引代替字符串键)
        std::vector<std::unordered_set<int>> did_sid; // demand id --> supply id set (使用索引代替字符串键)
        // std::vector<std::unordered_set<int>> canallocate_did_sid; // demand id --> 可分配的 supply id set (使用索引代替字符串键)
        std::vector<double> sid_heat; // supply id --> heat (使用索引代替字符串键)

        std::vector<int> sid_remainpv; // supply id --> remaining pv (使用索引代替字符串键)
        std::vector<std::unordered_map<int, int>> sid_did_allocatepv; // 分配结果，supply id --> demand id --> pv_amount (使用索引代替字符串键)
        std::vector<int> did_remain_amount;

        std::unordered_map<std::string, int> sid_index;
        std::unordered_map<std::string, int> did_index; 
        std::vector<std::string> index_sid; 
        std::vector<std::string> index_did; 

        // 并行相关成员变量（类似 MIQP 的 ParallelSolver）
        mutable boost::mutex parallel_mutex_;
        std::atomic<bool> parallel_running_{false};
        static const int NUM_THREADS = 32; 

        void read_demand_data();
        void read_heat_data();
        void read_supply_data();
        std::unordered_set<std::string> preload_valid_demands();  // 新增：预加载有效demand集合
        void model_problem(std::string allocation_res_file,  ModelMode mode, std::vector<std::string> online_query_list);
        std::vector<std::unordered_map<int, int>> LocalSearch(std::string allocation_res_file, ModelMode mode, std::vector<std::string> online_query_list,std::chrono::high_resolution_clock::time_point start_time);
        
        // 并行相关的成员函数声明
        void local_greedy(ParallelLocalSearchWorker* worker);
        std::vector<int> collect_local_unfinished_demands(ParallelLocalSearchWorker* worker);
        int local_optimization_parallel(ParallelLocalSearchWorker* worker, std::vector<int>& my_unfinished_demands);
        int move_pv_parallel(ParallelLocalSearchWorker* worker, std::vector<int>& unfinished_demands, 
                            std::unordered_set<int>& unfinished_needed_supply, double threshold);
        void local_optimize_obj_parallel(ParallelLocalSearchWorker* worker, double threshold);
        
        void greedy(std::vector<std::pair<int, double>> sid_list, std::vector<int> demand_rank);
        // void para_greedy(std::vector<std::pair<int, double>> sid_list, std::vector<int> demand_rank);
        bool para_local_search(std::vector<std::pair<int, double>>& sid_list, std::vector<int>& demand_rank, std::vector<int>& demand_count);
        
        // 并行局部搜索相关的辅助函数
        void initialize_parallel_workers(std::vector<ParallelLocalSearchWorker>& workers, 
                                        const std::vector<std::pair<int, double>>& sid_list,
                                        const std::vector<int>& demand_rank,
                                        const std::vector<int>& demand_count);
        bool check_global_feasibility(const std::vector<ParallelLocalSearchWorker>& workers);
        bool aggregate_worker_results(std::vector<ParallelLocalSearchWorker>& workers);
        void wait_for_all_phase_completion(std::vector<ParallelLocalSearchWorker>& workers);
        void wait_for_all_iteration_completion(std::vector<ParallelLocalSearchWorker>& workers);
        void LSout_offline(std::string offline_res_file);
        void LSout_online(std::string online_allocation_res_file);
        int online_query(std::vector<std::string> demand_ids);
        int query_remaining_pv_for_demand(std::string demand_id);
        int local_optimization(std::vector<int>& unfinished_demands);
        int move_pv(std::vector<int>& unfinished_demands,double threshold);
        int optimize_obj(std::vector<std::pair<int, double>> sid_list, std::vector<int> demand_rank,double threshold);
        int optimize_online(std::vector<std::string> demand_ids);

        // 连通分支相关方法
        struct ComponentInfo {
            std::vector<int> supply_indices;  // 该分支包含的supply索引
            std::vector<int> demand_indices;  // 该分支包含的demand索引
        };
        std::vector<ComponentInfo> find_components();
        void solve_single_component(const ComponentInfo& component, const std::string& output_file, 
                                          ModelMode mode, const std::vector<std::string>& online_query_list,
                                          std::chrono::high_resolution_clock::time_point start_time);

    public:
        sls_model(std::string supply_set, std::string demand_set, std::string heat_set);
        void solve_problem(std::string allocation_res_file,  ModelMode mode, std::vector<std::string> online_query_list);
        void solve_with_components(std::string output_base, ModelMode mode, std::vector<std::string> online_query_list);
    };
};