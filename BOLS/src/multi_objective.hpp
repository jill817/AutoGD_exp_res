#pragma once

/* inclusions *****************************************************************/

#include <assert.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <random>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <cmath>
#include "utils.hpp"
#include "assignment.hpp"

/* uses ***********************************************************************/
using std::ostream;
using std::cout;
using std::endl;
using std::string;
using std::to_string;
using std::vector;
using std::map;
using std::pair;



class Vec_with_index {
public:
	Vec_with_index() {}
	Vec_with_index(vector<long>::size_type max_sz) : vertex_index(max_sz, -1) {}
	void init(vector<long>::size_type max_sz) {
#ifndef NDEBUG
		//cout << "initializing Vertex_vec_with_index" << endl;
		//cout << max_sz << "max_sz" <<endl;
#endif
		vertex_index.clear();
		vertex_index.resize(max_sz, -1);
		vertex_vec.clear();
	}
	void push_back(long v) {
		if (vertex_index[v] != -1) return;
		vertex_index[v] = vertex_vec.size();
		vertex_vec.push_back(v);
	}
	void remove(long v) {
#ifndef NDEBUG
		if (vertex_index[v] == -1){
			cout << "remove error: " << v << " not exist" << endl;
			getchar();
		}
		//cout << "remove func debug " << vertex_index[*vertex_vec.rbegin()] << " " << vertex_vec[vertex_index[v]] << endl;
#endif
		vertex_index[*vertex_vec.rbegin()] = vertex_index[v];
		vertex_vec[vertex_index[v]] = *vertex_vec.rbegin();
		vertex_vec.pop_back();
		vertex_index[v] = -1;
	}
	bool exist(long v) {
		return vertex_index[v] != -1;
	}
	bool empty() {
		return vertex_vec.empty();
	}
	vector<long>::size_type size() {
		return vertex_vec.size();
	}
	vector<long>::size_type index(long v) {
		return vertex_index[v];
	}
	long & operator [] (vector<long>::size_type i) {
		return vertex_vec[i];
	}
	const long & operator [] (vector<long>::size_type i) const {
		return vertex_vec[i];
	}
	vector<long>::iterator begin() {
		return vertex_vec.begin();
	}
	vector<long>::iterator end() {
		return vertex_vec.end();
	}
	vector<long>::const_iterator begin() const {
		return vertex_vec.begin();
	}
	vector<long>::const_iterator end() const {
		return vertex_vec.end();
	}

private:
	vector<long> vertex_vec;
	vector<long> vertex_index;
};


// supply---->demand
//using 2 dimensional vector to store allocator

struct Solu
{
	long long obj1;
	double 	  obj2;
	bool	delete_flag = false;
	vector<vector<allocateVar>> assign_vec;

};



class OperatorPool;

class MultiObjectiveData
{
private:

	vector<long long> total_demand_supply_value;

	map<pair<string, string>,long long> supplyID2index;
	map<string, long long> 				demandID2index;

public:
	map<long long, std::pair<std::string, std::string>> index2supplyID;
	map<long long, string>				index2demandID;

    vector<vector<allocateVar>> assign_supply2demand;
    // vector<vector<long long>> allocat_supply2demand;
    vector<vector<long long>> allocat_position_in_supply;

    //inline bool haveSupply(string supplyID) const { return _supplyMap.count(supplyID) != 0;}
    inline bool haveDemand(string demandID) const { return demandID2index.count(demandID) != 0;}

public:
    long long supply_cnt = 0;
    long long demand_cnt = 0;
	long long supply_sum = 0;
	long long demand_sum = 0;

	long long BMS = 100;
	long long 	weight_obj1;
	vector<int> weight_supply;
	vector<int> weight_demand;

	// vector<vector<vector<allocateVar>>>	best_assign_vec;
	// vector<pair<long long, double>>		best_obj_value_vec;
	vector<Solu>						best_obj_value_vec;
	long long 							best_unsat_num;
	long long 							cur_unsat_num;
	

    long long unsat_supply;     
    long long unsat_demand;
    Vec_with_index unsat_supply_vec;
    Vec_with_index unsat_demand_vec;
	Vec_with_index query_supply_not_full;  //the supplys query which is not full


    vector<long long> supply_value;
    vector<long long> demand_value;
    vector<long long> supply_remain;
    vector<long long> demand_remain;

    vector<vector<long long>> map_supply2demand;
    vector<vector<long long>> map_demand2supply;

	vector<vector<double>> 	obj2_const_coef;		//const of objective 2
	vector<long long> 		obj1_query_supply;		//the supplys query
	vector<long long>		obj1_supply_flag;		//obj1_supply_flag[i] = 1 if  i in obj1_query_supply set, vice versa
	long long 				obj1_total_query_supply;//total supply of the query
	long long				obj1_available_query_supply;
	long long 				obj1_best_available_query_supply;
	vector<double> 			query_const_coef;		//const of objective 2 of query
	vector<long long> 		query_use;              //the use of queried supply 
	long long 				query_use_total = 0;

	OperatorPool operatorpool;

    MultiObjectiveData(){}
    //~MultiObjectiveData();

    long long getAlloPosition(long long demand_index1, long long num){ return allocat_position_in_supply[demand_index1][num]; }
    long long get_supply_value(long long supply_index1) { return supply_value[supply_index1]; }
    long long get_demand_value(long long demand_index1) { return demand_value[demand_index1]; }
    long long get_total_demand_supply_value(long long demand_index1) { return total_demand_supply_value[demand_index1]; }

    void readDemandFile(string filePath);
    void readSampleFile(string filePath);
    void show_data();

    void   init_allocation();
	bool init_solution();
	void   coonstruct_query();

	//to sat an unsat constraint of supply or demand
	void do_sat_supply_move(long long supply);
	// void do_sat_demand_move(long long demand);
	void do_sat_supply_move_2();
	void do_sat_demand_move_2();
	void do_sat_constraint_move();
	void do_sat_constraint_move_new();
	void do_sat_constraint_move_new2();
	void do_improve_balance_move();
	bool do_improve_balance_move2(long long demand);

	bool do_reduce_move();
	bool do_2step_reduce_move();
	bool do_2step_reduce_balance_move();
	bool do_2step_reduce_balance_move_new();
	// bool do_bound_move();
	// bool choose_from_operatorPool(long long min_score);
	
	// if is_supply = false, then the operator is demand
	// long long calcu_min_value_move(allocateVar var, bool is_supply);
	// long long calcu_max_value_move(allocateVar var, bool is_supply);
	long long calcu_operator_score(allocateVar var, long long var_val_new);
	long long calcu_2step_operator_score(allocateVar &var1, allocateVar &var2, long long trans_val);
	long long calcu_2step_operator_score_new(allocateVar &var1, allocateVar &var2, long long trans_val);
	// void update_constraint_weight();

    bool check_demand_sat_state(long long demand);
    bool check_supply_sat_state(long long supply);
    bool change_demand_sat_state(bool old_state, bool new_state, long long demand);
    bool change_supply_sat_state(bool old_state, bool new_state, long long supply);
    bool change_assignment(long long supply, long long supply_position, long long demand, long long new_val);

	double calcu_objective2_value();
	long long calcu_objective1_supply_query();

	void update_solution();
	bool update_solution2(double obj2_bound);
	// void delete_solution(long long num);
	void delete_solution();
	void incremental_update_solutioon();
	bool one_time_update_solution(long long query_demand_value);
	void push_assignment(long long num);

	void localsearch();

	bool check_solution(Solu solu);
	double hyper_volume();

	int objective_order();

	// bool do_2step_reduce_move();
	bool do_2step_improve_move();
	bool do_2step_reduce_move_new();
	bool do_2step_reduce_move_new_bal2(long long demand);

	bool do_1step_improve_move();
	bool do_1step_reduce_move_new();
	bool do_1step_reduce_move_new_bal2();

	double calcu_reduce_score();
};

