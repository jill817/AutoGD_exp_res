
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
            // online demand indicator (1 if did appears in online demand input)
            std::vector<int> did_online_mask;
            // last N valid demands (index and id) collected in file order
            std::vector<int> last_online_list;
            std::vector<std::string> last_online_idlist;
            size_t tail_keep = 50; // how many demands to keep from the tail of demand file
            // track the last valid demand encountered in demand file that is also present in supply-filtered set
            int last_valid_demand_idx = -1;
            std::string last_valid_demand_id;

            // online metrics
            long long origin_gain_online = 0; // total transferred PV to online demand (actual_transfer)
            long long unsatisfied_total = 0;  // sum of remaining demand after offline+online
            double penalty = 0.0;             // unsatisfied_total * 1000
            double gain_metric = 0.0;         // origin_gain_online - penalty

            std::unordered_map<std::string, int> sid_index;
            std::unordered_map<std::string, int> did_index;
            std::vector<std::string> index_sid;
            std::vector<std::string> index_did;

            // IO and parsing
            void read_demand_data();
            void read_heat_data();
            void read_supply_data();
            std::unordered_set<std::string> preload_valid_demands();
            void mark_fixed_online_demand();

            // core algorithm (serialized)
            void model_problem(std::string allocation_res_file, ModelMode mode, int cutoff_seconds);
            std::vector<std::unordered_map<int, int>> LocalSearch(std::string allocation_res_file, ModelMode mode);

            // int move_pv(std::vector<int>& unfinished_demands, double threshold);

            // output / online
            void LSout_offline(std::string offline_res_file);
            void LSout_online(std::string online_res_file);
            void FIFO_online(int cutoff_seconds);
            // int online_query(std::vector<std::string> demand_ids);
            // int optimize_online(std::vector<std::string> demand_ids);
            // int query_remaining_pv_for_demand(std::string demand_id);

        public:
            sls_model(std::string supply_set, std::string demand_set, std::string heat_set, size_t tail_keep = 50);
            void solve_problem(std::string allocation_res_file, ModelMode mode, int cutoff_seconds);


        };

    }