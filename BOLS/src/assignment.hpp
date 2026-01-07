#pragma once
#include "utils.hpp"

class allocateVar;

// struct Operator {
//     Variable _var;
//     Int      _val;
// }; 

class OperatorPool {
protected:
    vector<allocateVar> _vars;
    vector<long long>      _vals;
    long long              _size;

    Map<unsigned, Set<long long> >_operatorSet;
    //Map<unsigned, Set<long long> >_operatorSet;

public:
    OperatorPool() { _size = 0; }
    void clear();
    long long  size();
    void push(allocateVar var, long long val);
    bool empty() const;

    bool haveOperator(const allocateVar& var, const long long& val);
    void addOperator(const allocateVar& var, const long long& val);

    allocateVar varAt(long long index) const;
    long long valAt(long long index) const;

    void removeOpAt(long long index);

    void printOperators() const;
};


class allocateVar
{
private:

public:
    long long supply_index;
    long long demand_index;
    long long allocate_value;       
    //long long allocat_position_indemand;

    allocateVar(long long s_index, long long d_index, long long allocate_val){ 
        supply_index = s_index;
        demand_index = d_index;
        allocate_value = allocate_val;
        //allocat_position_indemand = allocate_posi;
    }
	allocateVar(){
		supply_index = 0;
		demand_index = 0;
		allocate_value = 0;
	}

	void operator = (const allocateVar& var){
		supply_index = var.supply_index;
		demand_index = var.demand_index;
		allocate_value = var.allocate_value;
	}
};
