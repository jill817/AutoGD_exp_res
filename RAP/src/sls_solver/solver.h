#pragma once

#include "util.h"

namespace solver {
    enum direction {
        up, down
    };


    class var {
    public:
        int                     m_index;
        long double             m_lb, m_ub;
        bool                    m_lower, m_upper;
        std::string             m_name;
        long double             m_coeff;
        int_vector              m_constraints;
        int                     m_cast_id;
        int                     m_pos_step, m_neg_step;
        int_vector              m_crucial_constraint;

        var(int index, std::string _name, long double _coeff, bool lower, bool upper, long double lb, long double ub): m_index(index), m_name(_name), m_lower(lower), m_upper(upper), m_lb(lb), m_ub(ub), m_coeff(_coeff),
        m_cast_id(0), m_pos_step(0), m_neg_step(0)
        {
            m_constraints.clear();
        }

        void add_constraint(int x) {
            m_constraints.push_back(x);
        }
    };

    using var_vector = std::vector<var>;

    // class assignment {
    // private:
    //     std::unordered_map<int, long double> m_map;
    // public:
    //     assignment() {
    //         m_map.clear();
    //     }
        
    //     void set(int x, long double v) {
    //         m_map[x] = v;
    //     }

    //     long double value(int x) const {
    //         return m_map.at(x);
    //     }

    //     void shift(int v, long double s) {
    //         m_map[v] += s;
    //     }

    //     int size() const {
    //         return m_map.size();
    //     }

    //     bool empty() const {
    //         return m_map.empty();
    //     }

    //     void reset() {
    //         m_map.clear();
    //     }

    //     // void copy(assignment const & ass) {
    //     //     m_map.clear();
    //     //     for(auto ele: ass.m_map) {
    //     //         m_map[ele.first] = ele.second;
    //     //     }
    //     // }

    //     bool eq(assignment const & ass) const {
    //         for(auto & v: m_map) {
    //             if(ass.m_map.count(v.first) == 0 || v.second != ass.m_map.at(v.first)) {
    //                 return false;
    //             }
    //         }
    //         return true;
    //     }
    // };

    class monomial {
    public:
        int_table m_vars;
        long double m_coeff;

        monomial(long double _coeff, int_table const & vec): m_coeff(_coeff), m_vars(vec) {}
        monomial(monomial const & mono): monomial(mono.m_coeff, mono.m_vars) {}
        // monomial except var v
        monomial(monomial const & mono, int v): monomial(mono) 
        {
            m_vars.erase(v);
        }

        // void copy(monomial const & mono) {
        //     m_vars.clear();
        //     for(auto ele: mono.m_vars) {
        //         m_vars.insert(ele);
        //     }
        //     m_coeff = mono.m_coeff;
        // }

        int size() const {
            return m_vars.size();
        }

        bool is_const() const {
            return size() == 0;
        }

        bool is_linear() const {
            return size() == 1;
        }

        bool contains_var(int x) const {
            return m_vars.count(x) != 0;
        }

        // long double calculate_sum(assignment const & m_assignment) const {
        //     long double res = m_coeff;
        //     for(int v: m_vars) {
        //         res *= m_assignment.value(v);
        //     }
        //     return res;
        // }
        
        // calculate value without that var
        // i.e. 2 x1 * x2, with {x1 = 1, x2 = 2}
        // f(x1) = 4, f(x2) = 2
        // long double calculate_value_except(int v, assignment const & m_assignment) const {
        //     ASSERT(m_vars.count(v) != 0);
        //     long double res = m_coeff;
        //     for(int x: m_vars) {
        //         if(x == v) {
        //             continue;
        //         }
        //         res *= m_assignment.value(x);
        //     }
        //     return res;
        // }

        void mul(monomial const & mono) {
            for(int v: mono.m_vars) {
                if(m_vars.count(v) == 0) {
                    m_vars.insert(v);
                }
            }
            m_coeff *= mono.m_coeff;
        }

        monomial operator*(monomial const & other) {
            monomial res(*this);
            res.mul(other);
            return res;
        }
    };

    using monomial_vector = std::vector<monomial>;

    class polynomial {
    public:
        monomial_vector                        m_monomials;
        int_vector                             m_linear_terms, m_nonlinear_terms;
        long double                            m_sum;
        std::unordered_map<int, int_table>     m_var_neighbours;

        polynomial(): has_store_vars(false) {
            m_monomials.clear();
            m_linear_terms.clear();
            m_nonlinear_terms.clear();
            m_sum = 0;
            m_var_neighbours.clear();
        }

        polynomial(monomial_vector const & vec): has_store_vars(false) {
            m_monomials.clear();
            m_linear_terms.clear();
            m_nonlinear_terms.clear();
            int index = 0;
            for(auto ele: vec) {
                m_monomials.push_back(monomial(ele));
                if(ele.is_linear()) {
                    m_linear_terms.push_back(index);
                } else {
                    m_nonlinear_terms.push_back(index);
                }
                index++;
            }
            m_sum = 0;
            m_var_neighbours.clear();
        }

        polynomial(polynomial const & poly): polynomial(poly.m_monomials) {}

        polynomial(std::vector<long double> const & coeff_vec, std::vector<int_table> const & mono_vars): has_store_vars(false) {
            ASSERT(coeff_vec.size() == mono_vars.size());
            m_monomials.clear();
            m_linear_terms.clear();
            m_nonlinear_terms.clear();
            m_sum = 0;
            for(int i = 0; i < coeff_vec.size(); i++) {
                m_monomials.push_back(monomial(coeff_vec[i], mono_vars[i]));
                if(mono_vars[i].size() == 1) {
                    m_linear_terms.push_back(i);
                } else {
                    m_nonlinear_terms.push_back(i);
                }
            }
            m_var_neighbours.clear();
        }

        void set_sum(long double _sum) {
            m_sum = _sum;
        }

        long double get_sum() const {
            return m_sum;
        }

        int size() const {
            return m_monomials.size();
        }

        void add(monomial const & mono) {
            int index = m_monomials.size();
            m_monomials.push_back(monomial(mono));
            if(mono.is_linear()) {
                m_linear_terms.push_back(index);
            } else {
                m_nonlinear_terms.push_back(index);
            }
        }

        polynomial operator+(monomial const & mono) {
            polynomial res(*this);
            res.add(mono);
            return res;
        }

        void add(polynomial const & poly) {
            for(auto ele: poly.m_monomials) {
                add(ele);
            }
        }

        polynomial operator+(polynomial const & poly) {
            polynomial res(*this);
            res.add(poly);
            return res;
        }

        void sub(monomial const & mono) {
            int index = m_monomials.size();
            m_monomials.push_back(monomial(-1.0 * mono.m_coeff, mono.m_vars));
            if(mono.is_linear()) {
                m_linear_terms.push_back(index);
            } else {
                m_nonlinear_terms.push_back(index);
            }
        }

        polynomial operator-(monomial const & mono) {
            polynomial res(*this);
            res.sub(mono);
            return res;
        }

        void sub(polynomial const & poly) {
            for(auto ele: poly.m_monomials) {
                sub(ele);
            }
        }

        polynomial operator-(polynomial const & poly) {
            polynomial res(*this);
            res.sub(poly);
            return res;
        }

        void mul(monomial const & mono) {
            for(auto ele: m_monomials) {
                ele.mul(mono);
            }
            adjust_linear_nonlinear();
        }

        polynomial operator*(monomial const & mono) {
            polynomial res(*this);
            res.mul(mono);
            return res;
        }

        void mul(polynomial const & poly) {
            monomial_vector m_mono;
            for(auto ele1: m_monomials) {
                for(auto ele2: poly.m_monomials) {
                    m_mono.push_back(monomial(ele1 * ele2));
                }
            }
            m_monomials.clear();
            for(auto ele: m_mono) {
                m_monomials.push_back(ele);
            }
            adjust_linear_nonlinear();
        }

        polynomial operator*(polynomial const & poly) {
            polynomial res(*this);
            res.mul(poly);
            return res;
        }

        bool contains(int v) const {
            return m_vars.count(v) != 0;
        }

        // store vars contained
        int_table m_vars;
        // var -> monomial terms which contain var
        std::unordered_map<int, int_vector> m_var_factor;
        // var -> coeff value
        std::unordered_map<int, long double> m_var_factor_value;
        // whether store vars before
        bool has_store_vars;
        // var --> factor vars, intercept vars
        std::unordered_map<int, std::pair<int_table, int_table>> m_var_vars;

        // void store_vars_and_generate_factor() {
        //     if(has_store_vars) {
        //         return;
        //     }
        //     has_store_vars = true;
        //     m_vars.clear();
        //     m_var_factor.clear();
        //     // Generate var factor
        //     for(int i = 0; i < m_monomials.size(); i++) {
        //         auto mono = m_monomials[i];
        //         for(int v: mono.m_vars) {
        //             if(m_vars.count(v) == 0) {
        //                 m_vars.insert(v);
        //             }
        //             m_var_factor[v].push_back(i);
        //         }
        //     }
        //     m_var_vars.clear();
        //     m_stored_factor.clear();
        // }

        /*
        // 未被 main 及其调用链调用，已注释
        void push_table_into(int_table const & t1, int_table & t2) {
            for(auto v: t1) {
                if(t2.count(v) == 0) {
                    t2.insert(v);
                }
            }
        }

        void store_factor_and_intercept_all_vars() {
            m_var_vars.clear();
            for(int v: m_vars) {
                m_var_vars[v] = std::make_pair(int_table(), int_table());
            }
            for(int i = 0; i < m_monomials.size(); i++) {
                for(int v: m_vars) {
                    if(m_monomials[i].m_vars.count(v) == 0) {
                        push_table_into(m_monomials[i].m_vars, m_var_vars[v].second);
                    } else {
                        push_table_into(m_monomials[i].m_vars, m_var_vars[v].first);
                    }
                }
            }
        }

        std::unordered_set<int> m_stored_factor;

        void store_factor_and_intercept_vars(int v) {
            if(m_stored_factor.count(v) != 0) {
                return;
            }
            m_var_vars[v] = std::make_pair(int_table(), int_table());
            for(int i = 0; i < m_monomials.size(); i++) {
                if(m_monomials[i].m_vars.count(v) == 0) {
                    for(auto ele: m_monomials[i].m_vars) {
                        if(m_var_vars[v].second.count(ele) == 0) {
                            m_var_vars[v].second.insert(ele);
                        }
                    }
                } else {
                    for(auto ele: m_monomials[i].m_vars) {
                        if(ele == v) {
                            continue;
                        }
                        if(m_var_vars[v].first.count(ele) == 0) {
                            m_var_vars[v].first.insert(ele);
                        }
                    }
                }
            }
            m_stored_factor.insert(v);
        }
        */

        /*
        // 未被项目调用，已注释
        void generate_var_neighbours() {
            for (int i = 0; i < m_nonlinear_terms.size(); i++) {
                auto idx = m_nonlinear_terms[i];
                auto mono = m_monomials[idx];
                ASSERT(mono.m_vars.size() >= 2);
                for (int v1 : mono.m_vars) {
                    for (int v2 : mono.m_vars) {
                        if (v1 == v2) {
                            continue;
                        }
                        if (m_var_neighbours[v1].count(v2) == 0) {
                            m_var_neighbours[v1].insert(v2);
                        }
                    }
                }
            }
        }
        */

        /*
        // 未被 main 及其调用链调用，已注释
        void generate_factor_values(assignment const & m_assignment) {
            m_var_factor_value.clear();
            for(int v: m_vars) {
                generate_factor_values_core(v, m_assignment);
            }
        }
        */

        // long double calculate_sum(assignment const & m_assignment) const {
        //     long double res = 0;
        //     for(int i = 0; i < m_monomials.size(); i++) {
        //         res += m_monomials[i].calculate_sum(m_assignment);
        //     }
        //     return res;
        // }

        /*
        // 未被 main 及其调用链调用，已注释
        void generate_factor_values_core(int v, assignment const & m_assignment) {
            m_var_factor_value[v] = calculate_var_factor_value(v, m_assignment);
        }
        */

        
    private:
        // long double calculate_var_factor_value(int v, assignment const & m_assignment) const {
        //     long double sum = 0;
        //     for(auto idx: m_var_factor.at(v)) {
        //         sum += m_monomials[idx].calculate_value_except(v, m_assignment);
        //     }
        //     return sum;
        // }

        void adjust_linear_nonlinear() {
            m_linear_terms.clear();
            m_nonlinear_terms.clear();
            for(int i = 0; i < m_monomials.size(); i++) {
                if(m_monomials[i].is_linear()) {
                    m_linear_terms.push_back(i);
                } else {
                    m_nonlinear_terms.push_back(i);
                }
            }
        }
    };

    enum constraint_kind {
        BOUND, CAST, DEMAND, DEMAND_COMPARE, OTHER
    };

    class constraint {
    public:
        polynomial         m_poly;
        int                m_index;
        bool               m_lower, m_upper;
        long double        m_lower_bound, m_upper_bound;
        bool               is_sat, is_bounded;
        bool               is_lower_sat, is_upper_sat;
        long double        m_lower_weight, m_upper_weight;
        std::string        m_name;
        constraint_kind    m_kind;
        double             m_learning_rate;
        
        constraint(int index, std::string _name, constraint_kind _kind, polynomial const & poly, bool lower, bool upper, long double lb, long double ub): m_poly(poly), m_index(index),
        m_lower(lower), m_upper(upper), m_lower_bound(lb), m_upper_bound(ub), is_sat(false), is_bounded(false), m_lower_weight(1.0), m_upper_weight(1.0),
        m_name(_name), is_lower_sat(false), is_upper_sat(false), m_kind(_kind)
        {
        }

        /*
        // 未被 main 及其调用链调用，已注释
        int get_index() const {
            return m_index;
        }

        bool get_sat_status() const {
            return is_sat;
        }

        bool get_bound_status() const {
            return is_bounded;
        }

        bool get_lower_sat() const {
            return is_lower_sat;
        }

        bool get_upper_sat() const {
            return is_upper_sat;
        }

        void set_sat_status(bool b) {
            is_sat = b;
        }

        void set_lower_sat(bool b) {
            is_lower_sat = b;
        }

        void set_upper_sat(bool b) {
            is_upper_sat = b;
        }

        void set_bound_status(bool b) {
            is_bounded = b;
        }

        long double get_lower_weight() const {
            return m_lower_weight;
        }

        long double get_upper_weight() const {
            return m_upper_weight;
        }

        void inc_lower_weight() {
            m_lower_weight++;
        }

        void inc_upper_weight() {
            m_upper_weight++;
        }
        */
    };

    using constraint_vector = std::vector<constraint *>;




    class opt_solver {
    public:
        struct imp;
    private:
        imp * m_imp;
    public:
        opt_solver();

        // input
        int register_var(std::string str, long double _coeff, bool lower, bool upper, long double lb, long double ub);
        void register_constraint(polynomial const & poly, constraint_kind _kind, std::string _name, bool lower, bool upper, long double lb, long double ub);

        // getters for RAP in-memory
        const var_vector& get_vars() const;
        const constraint_vector& get_constraints() const;
    };
};