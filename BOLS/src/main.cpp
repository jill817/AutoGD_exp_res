
#include <cstddef>
#include <cstring>
#include "multi_objective.hpp"


void print_best_single_solution(const MultiObjectiveData& multi_data, const std::string& output_base) {
    // 1. 输出所有帕累托解的 obj1 和 obj2
    int best_idx = -1;
    long long best_obj = std::numeric_limits<long long>::min();
    std::cout << "Pareto solutions (obj1, obj2):" << std::endl;
    for (size_t i = 0; i < multi_data.best_obj_value_vec.size(); ++i) {
        long long obj1 = multi_data.best_obj_value_vec[i].obj1;
        double obj2 = multi_data.best_obj_value_vec[i].obj2;
        long long obj = obj1 - obj2;
        std::cout << obj1 << " " << obj2 << std::endl;
        if (obj > best_obj) {
            best_obj = obj;
            best_idx = i;
        }
    }
    if (best_idx == -1) {
        std::cout << "No feasible solution found." << std::endl;
        return;
    }
    // 2. 输出最优 obj
    std::cout << "Best OBJ: " << best_obj << std::endl;

    // 3. 输出最优解的方案文件
    std::string filename = output_base + "_offline.csv";
    std::cout << "Writing best solution to: " << filename << std::endl;
    std::ofstream fout(filename);
    if (!fout.is_open()) {
        std::cout << "Failed to open file " << filename << std::endl;
        return;
    }
    fout << "SID,Demand,Allocated PV\n";
    const auto& assign_vec = multi_data.best_obj_value_vec[best_idx].assign_vec;
    for (std::size_t s = 0; s < assign_vec.size(); ++s) {
        for (std::size_t d = 0; d < assign_vec[s].size(); ++d) {
            const auto& var = assign_vec[s][d];
            if (var.allocate_value > 0) {
                std::string usr_id, supply_id;
                if (multi_data.index2supplyID.count(var.supply_index)) {
                    usr_id = multi_data.index2supplyID.at(var.supply_index).first;
                    supply_id = multi_data.index2supplyID.at(var.supply_index).second;
                } else {
                    usr_id = "unknown_usr";
                    supply_id = std::to_string(var.supply_index);
                }
                std::string demand_id = multi_data.index2demandID.count(var.demand_index) ?
                                        multi_data.index2demandID.at(var.demand_index) :
                                        std::to_string(var.demand_index);
                fout << usr_id << "_" << supply_id << "," << demand_id << "," << var.allocate_value << "\n";
            }
        }
    }
    fout.close();
}

void print_current_obj(MultiObjectiveData& multi_data)
{
    cout << "unsat supply: " << multi_data.unsat_supply << endl;
    long long unsat_supply_sum = 0;
    for (auto supply : multi_data.unsat_supply_vec)
    {
        unsat_supply_sum += multi_data.supply_remain[supply];
        cout << supply << " " << multi_data.supply_value[supply] << " " << multi_data.supply_remain[supply] << endl;
    }
    cout << "unsat supply sum: " << unsat_supply_sum << endl;

    // 重新计算当前assign_supply2demand下的obj1/obj2
    // 1. 计算 query_use/query_use_total
    std::vector<long long> query_use;
    long long query_use_total = 0;
    for (size_t i = 0; i < multi_data.obj1_query_supply.size(); ++i) {
        long long supply = multi_data.obj1_query_supply[i];
        long long supply_provide = 0;
        for (size_t j = 0; j < multi_data.assign_supply2demand[supply].size(); ++j) {
            supply_provide += multi_data.assign_supply2demand[supply][j].allocate_value;
        }
        query_use.push_back(supply_provide);
        query_use_total += supply_provide;
    }

    // 2. 计算 obj2
    double obj2 = multi_data.calcu_objective2_value();
    for (size_t i = 0; i < multi_data.obj1_query_supply.size(); ++i) {
        obj2 += std::abs((double)multi_data.query_const_coef[i] * query_use_total - query_use[i]);
    }

    // 3. 计算 obj1
    long long obj1 = 0;
    for (size_t i = 0; i < query_use.size(); ++i) {
        obj1 += query_use[i];
    }

    std::cout << obj1 << " " << obj2 << std::endl;
    std::cout << "OBJ: " << (obj1 - obj2 + unsat_supply_sum) << std::endl;
}

void print_best_solution(const MultiObjectiveData& multi_data, std::string output_base) 
{
    std::cout << "print_best_solution: candidate count = " << multi_data.best_obj_value_vec.size() << std::endl;
    if (multi_data.best_obj_value_vec.empty()) {
        std::cout << "print_best_solution: no solutions to write" << std::endl;
        return;
    }

    for (std::size_t i = 0; i < multi_data.best_obj_value_vec.size(); ++i) {
        std::string filename = output_base + "_" + std::to_string(i+1) + "offline.csv";
        std::cout << "print_best_solution: writing " << filename << std::endl;
        std::ofstream fout(filename);
        if (!fout.is_open()) {
            std::cout << "print_best_solution: failed to open file " << filename << std::endl;
            continue;
        }
        fout << "SID,Demand,Allocated PV\n";
        const auto& assign_vec = multi_data.best_obj_value_vec[i].assign_vec;
        for (std::size_t s = 0; s < assign_vec.size(); ++s) {
            for (std::size_t d = 0; d < assign_vec[s].size(); ++d) {
                const auto& var = assign_vec[s][d];
                if (var.allocate_value > 0) {
                    std::string usr_id, supply_id;
                    if (multi_data.index2supplyID.count(var.supply_index)) {
                        usr_id = multi_data.index2supplyID.at(var.supply_index).first;
                        supply_id = multi_data.index2supplyID.at(var.supply_index).second;
                    } else {
                        cout << "Invalid supply index: " << var.supply_index << endl;
                    }
                    std::string demand_id = multi_data.index2demandID.count(var.demand_index) ? multi_data.index2demandID.at(var.demand_index) : std::to_string(var.demand_index);
                    fout << usr_id << "_" << supply_id << "," << demand_id << "," << var.allocate_value << "\n";
                }
            }
        }
        fout.close();
    }
}

// Dump the current assignment even if infeasible, helpful for debugging.
void print_current_assignment(const MultiObjectiveData& multi_data, std::string output_base)
{
    std::string filename = output_base + "_offline.csv";
    std::cout << "print_current_assignment: writing " << filename << std::endl;
    std::ofstream fout(filename);
    if (!fout.is_open()) {
        std::cout << "print_current_assignment: failed to open file " << filename << std::endl;
        return;
    }
    fout << "SID,Demand,Allocated PV\n";
    for (std::size_t s = 0; s < multi_data.assign_supply2demand.size(); ++s) {
        const auto& row = multi_data.assign_supply2demand[s];
        for (std::size_t p = 0; p < row.size(); ++p) {
            const auto& var = row[p];
            if (var.allocate_value <= 0) continue;
            std::string usr_id, supply_id;
            if (multi_data.index2supplyID.count(var.supply_index)) {
                usr_id = multi_data.index2supplyID.at(var.supply_index).first;
                supply_id = multi_data.index2supplyID.at(var.supply_index).second;
            } else {
                usr_id = "unknown_usr";
                supply_id = std::to_string(var.supply_index);
            }
            std::string demand_id = multi_data.index2demandID.count(var.demand_index) ?
                                    multi_data.index2demandID.at(var.demand_index) :
                                    std::to_string(var.demand_index);
            fout << usr_id << "_" << supply_id << "," << demand_id << "," << var.allocate_value << "\n";
        }
    }
    fout.close();

}

int main(int argc, char** argv) {
    util::setRandom(DEFAULT_RANDOM_SEED);
    TimePoint start_time = util::getTimePoint();

    double time_limit = 60;
    cout << "time limit: " << time_limit << endl;
    // time_limit = atoi(argv[3]) * 0.9 - 3;

    long long no_update1 = 0;
    long long no_update2 = 0;
    long long no_update_max1 = 10;
    long long no_update_max2 = 10;
    // if (argc >= 6) {
    //     no_update_max1 = atoi(argv[4]);
    //     no_update_max2 = atoi(argv[5]);
    // }

    
    cout << "parameter: " << no_update_max1 << " " << no_update_max2 << endl;


    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " demand_file sample_file heat_file output_base" << std::endl;
        return 1;
    }

    std::string demand_file = argv[1];
    std::string sample_file = argv[2];
    std::string heat_file = argv[3];
    std::string output_base = argv[4];

    MultiObjectiveData multi_data;

    // TimePoint tp = util::getTimePoint();
    multi_data.readDemandFile(demand_file);
    // cout << "readDemandFile time: " << util::getSeconds(tp) << " s" << endl;

    // tp = util::getTimePoint();
    multi_data.readSampleFile(sample_file);
    // cout << "readSampleFile time: " << util::getSeconds(tp) << " s" << endl;

    // tp = util::getTimePoint();
    multi_data.coonstruct_query();
    // cout << "construct_query time: " << util::getSeconds(tp) << " s" << endl;

    // tp = util::getTimePoint();
    multi_data.init_allocation();
    // cout << "init_allocation time: " << util::getSeconds(tp) << " s" << endl;

    // long long mode = multi_data.objective_order();
    long long mode = 1;

    for (long long i = 0; i < 10000000; i++){
        // cout << i << endl;
        //multi_data.update_solution();
        //multi_data.do_improve_balance_move();

        if (multi_data.unsat_demand == 0 &&multi_data.unsat_supply == 0){

            multi_data.obj1_available_query_supply = multi_data.calcu_objective1_supply_query();
            // cout << "available: " << multi_data.obj1_available_query_supply << endl;
            if (multi_data.obj1_best_available_query_supply < multi_data.obj1_available_query_supply){     
                // no_impr = 0;           
                multi_data.obj1_best_available_query_supply = multi_data.obj1_available_query_supply;
            }

            if (mode == 1){
                bool update_flag = false;
                if (multi_data.init_solution()){
                    update_flag = true;
                }
                for (int i = 0; i <= 100; i++){
                    if (util::getSeconds(start_time) > time_limit) break;
                    assert(100 != 0);
                    assert(multi_data.obj1_available_query_supply >= 0); // 允许为0但提醒
                    long long query_supply_use = multi_data.obj1_available_query_supply * i / 100;
                    if (multi_data.one_time_update_solution(query_supply_use)){
                        update_flag = true;
                    }
                }
                if (util::getSeconds(start_time) > time_limit) break;
                if (update_flag == false) no_update1++;

                if (mode == 1 && no_update1 >= no_update_max1) {
                    mode = 2;
                    no_update1 = 0;
                } 
            }

            else if (mode == 2){
                bool update_flag = false;
                if (multi_data.init_solution()){
                    update_flag = true;
                }
                for (int i = 0; i <= 100; i++){
                    if (util::getSeconds(start_time) > time_limit) break;
                    long long query_supply_use = multi_data.obj1_available_query_supply * i / 100;
                    if (multi_data.one_time_update_solution(query_supply_use)){
                        update_flag = true;
                    }
                }
                if (util::getSeconds(start_time) > time_limit) break;
                if (update_flag == false) no_update2++;

                if (mode == 2 && no_update2 >= no_update_max2) {
                    mode = 1;
                    no_update2 = 0;
                } 
            }
            else{
                
                multi_data.init_solution();
                for (int i = 0; i <= 100; i++){
                    if (util::getSeconds(start_time) > time_limit) break;
                    long long query_supply_use = multi_data.obj1_available_query_supply * i / 100;
                    multi_data.one_time_update_solution(query_supply_use);
                }
            }
            
            // for (int i = 0; i <= 100; i++){
            //     if (util::getSeconds(start_time) > time_limit) break;
            //     long long query_supply_use = multi_data.obj1_available_query_supply * i / 100;
            //     multi_data.one_time_update_solution(query_supply_use);
            // }
        }
        
        // if (multi_data.unsat_demand == 0 &&multi_data.unsat_supply == 0) multi_data.do_2step_reduce_move();
        if (multi_data.unsat_demand == 0 &&multi_data.unsat_supply == 0){
            if (mode == 1){
                if (multi_data.do_1step_improve_move()){

                }
                else if (multi_data.do_2step_improve_move()){

                }
                else if (multi_data.do_1step_reduce_move_new()){

                }
                else {
                    multi_data.do_2step_reduce_move_new();
                }
            }
            if (mode == 2){
                long long demand = rand() % multi_data.demand_cnt;
                if (multi_data.do_1step_improve_move()){

                }
                else if (multi_data.do_improve_balance_move2(demand)){

                }
                else if (multi_data.do_1step_reduce_move_new_bal2()){
                    
                }
                else{
                    multi_data.do_2step_reduce_move_new_bal2(demand);
                }
            }

            // no_impr++;
        }
            // multi_data.do_2step_reduce_balance_move_new();

        multi_data.do_sat_constraint_move_new();

        // multi_data.update_constraint_weight();
        if (util::getSeconds(start_time) > time_limit) break;
    }


    if (multi_data.unsat_supply <= 0 && multi_data.unsat_demand <= 0) 
    {
        cout << "find feasible solution " << endl;
        print_best_single_solution(multi_data, output_base);
    }
    else 
    {
        cout << "not find feasible solution " << endl; 
        print_current_obj(multi_data);
        

        // dump current (possibly infeasible) assignment and summary
        print_current_assignment(multi_data, output_base);
    }

    // cout << "time: " << util::getSeconds(start_time) << " limit " << time_limit << endl;
    // multi_data.hyper_volume();

    return 0;   

}
