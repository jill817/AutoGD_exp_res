#pragma once

#include "solver.h"
#include "mode.h"
#include<vector>
#include <chrono>

/**
 * @brief Data Format Description
 * 1. integer dataset:  user id | supply id | cast times | demand_list (split by ,)
 * 2. demand dataset: demand id | demand amount
 */
namespace solver {
    // 未被使用的辅助结构，注释避免误用
    // struct supply_demands {
    //     int supply_id, demand_id1, demand_id2;
    //     supply_demands(int _supply, int demand1, int demand2): supply_id(_supply), demand_id1(demand1), demand_id2(demand2)
    //     {
    //
    //     }
    //
    //     bool operator==(supply_demands const & other) const {
    //         return supply_id == other.supply_id && (
    //             (demand_id1 == other.demand_id1 && demand_id2 == other.demand_id2) 
    //             ||
    //             (demand_id1 == other.demand_id2 && demand_id2 == other.demand_id1)
    //         );
    //     }
    // };
    // using supply_demands_set = std::unordered_set<supply_demands *>;

    class sls_model {
    private:
        opt_solver m_sls_solver;

        std::string format_supply_path, demand_path, heat_path;

        std::unordered_map<std::string, int> demand_id_amount; // demand id --> demand amount
        std::unordered_map<std::string, std::unordered_map<std::string, int> > user_supply_cast_times; // user --> supply --> cast times
        std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string> > > user_supply_demand_set; // user --> supply --> demand lists
        std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string> > > supply_user_demand_set; // supply --> user --> demand lists
        std::unordered_map<std::string, std::unordered_map<std::string, double>> user_supply_heat;// user_id --> supply_id --> heat


        std::unordered_map<std::string, int> var_name_index; // var string --> var index
        std::unordered_map<std::string, std::vector<int> > demand_vars; // demand id --> var index
        std::unordered_map<std::string, std::unordered_set<std::string> > supply_demand_set; // supply --> demand lists
        std::unordered_set<std::string> supply_with_more_demand;

        // 索引化存储（便于离线结果到在线阶段的映射）
        std::unordered_map<std::string, int> sid_index;           // "user_supply" -> idx
        std::unordered_map<std::string, int> did_index;           // demand_id -> idx
        std::vector<std::string> index_sid;                       // idx -> "user_supply"
        std::vector<std::string> index_did;                       // idx -> demand_id
        std::vector<int> sid_pv;                                  // 供应容量
        std::vector<int> sid_remainpv;                            // 剩余供应（在线前重置为 sid_pv）
        std::vector<int> did_remain_amount;                       // 剩余需求（在线前重置为 demand amount）
        std::vector<std::unordered_map<int, int>> sid_did_allocatepv; // 分配矩阵 sid -> (did -> pv)
        std::vector<std::unordered_set<int>> sid_did;             // sid -> 可投放的 did 集
        std::vector<int> did_online_mask;                         // 在线需求标记
        std::string last_valid_demand_id;                         // 记录在 demand 文件中出现的最后一个有效需求
        std::vector<std::string> demand_read_order;               // 记录 demand 读取顺序，便于确定 online 需求
        // int online_top_k;                                         // 在线阶段保留的需求数量
        long long origin_gain_online = 0;
        long long unsatisfied_total = 0;
        double penalty = 0.0;
        double gain_metric = 0.0;

        // id convert
        std::unordered_map<std::string, std::string> supply_id_convert, demand_id_convert, user_id_convert;

        void read_demand_data();
        void read_heat_data();
        void read_integer_data();
        void generate_id_convert();
        void model_problem(std::string lp_file, std::string sol_file, int nia_num, std::string result_file, ModelMode mode,int time_limit, int online_top_k);

        // 离线解到在线阶段的准备与输出
        void init_allocation_state();
        void reset_allocation_state();
        void map_solution_to_allocation(const std::vector<std::string>& var_list, const std::vector<double>& x,int online_top_k);
        void LSout_offline(const std::string& offline_res_file);
        void mark_fixed_online_demand(int online_top_k);
        void FIFO_online(int cutoff_seconds);
        void LSout_online(const std::string& online_res_file);
        void compute_and_log_online_metrics();
        void compute_and_log_offline_demand_totals(int online_top_k) const;
        std::vector<std::string> choose_online_demand_ids(int online_top_k) const;
    public:
        sls_model(std::string integer_set, std::string demand_set, std::string heat_set);
        void solve_problem(std::string lp_file,std::string sol_path, int nia_num, std::string, ModelMode mode,int time_limit, int online_top_k);
        // void write_solution(const std::string &filename, const std::vector<std::string> &var_list, const std::vector<double> &x);
        /*
        // 未被 main 及其调用链调用，已注释
        void solve_with_rap(const std::string& lp_file_path, const std::string& sol_file, ModelMode mode, const std::chrono::steady_clock::time_point& start_time, int time_limit);
        */
        void solve_with_rap_from_model(const solver::opt_solver& solver, const std::string& sol_file, ModelMode mode, const std::chrono::steady_clock::time_point& start_time, int time_limit, int online_top_k);
        /*
        // 未被 main 及其调用链调用，已注释
        void solve_with_gurobi(const std::string& lp_file_path, ModelMode mode, int time_limit = 3600, int method = 1, int threads = 128);
        */
        // void demo_solve(std::string str);
        // void demo_write_lp(std::string str);
    };
};