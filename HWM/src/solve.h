
    #pragma once

    #include "mode.h"
    #include <unordered_set>
    #include <vector>
    #include <unordered_map>
    #include <string>
    #include "util.h"

    namespace solver {

        // Split a string by the given delimiter into the provided vector
        void split(std::string &str, std::vector<std::string> &vec, char x);

        class sls_model {
        private:
            std::string format_supply_path, demand_path, heat_path;

            // core data structures
            std::vector<int> did_amount;
            std::vector<int> sid_pv;
            std::vector<std::unordered_set<int>> sid_did;
            std::vector<std::unordered_set<int>> did_sid;
            std::vector<double> sid_heat;

            std::vector<int> sid_remainpv;
            std::vector<std::unordered_map<int, int>> sid_did_allocatepv;
            std::vector<int> did_remain_amount;

            std::unordered_map<std::string, int> sid_index;
            std::unordered_map<std::string, int> did_index;
            std::vector<std::string> index_sid;
            std::vector<std::string> index_did;

            // IO and parsing
            void read_demand_data();
            void read_heat_data();
            void read_supply_data();
            std::unordered_set<std::string> preload_valid_demands();

            // core algorithm (serialized)
            void model_problem(std::string allocation_res_file, ModelMode mode, std::vector<std::string> online_query_list);
            std::vector<std::unordered_map<int, int>> LocalSearch(std::string allocation_res_file, ModelMode mode, std::vector<std::string> online_query_list);

            // int move_pv(std::vector<int>& unfinished_demands, double threshold);

            // output / online
            void LSout_offline(std::string offline_res_file);
            // void LSout_online(std::string online_allocation_res_file);
            // int online_query(std::vector<std::string> demand_ids);
            // int optimize_online(std::vector<std::string> demand_ids);
            // int query_remaining_pv_for_demand(std::string demand_id);

        public:
            sls_model(std::string supply_set, std::string demand_set, std::string heat_set);
            void solve_problem(std::string allocation_res_file, ModelMode mode, std::vector<std::string> online_query_list);


        };

    }