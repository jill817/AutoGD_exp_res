#include "solver.h"

#include <iostream>
#include <fstream>
#include <ctime>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <chrono>
#include <sys/types.h>
#include <string>

/**
 * @todo
 * 1. initialize greedy: count first, demand amount next
 * 2. adjust factor polynomial's value when inserting new var
 * 3. insert operation, floor or ceil
 * 4. in case > <, insert value
 *
 * @todo
 * 1. object weight = 0 before find sat, set 1 when find sat
 * 2. store var-constraint
 *
 * @date 23/05/09
 * @todo (code review)
 * 1. check solution sat
 * 2. lp file write object function, space before -1
 * 3. unbounded speed: BMS or other algorithms
 * 4. read lp format function
 *
 * @date 23/05/26
 * @todo (code review) VND
 * 1. for each unsat constraint, insert n-dimensional operation
 * 2. two level: first select var-pair which do not factor each other, second select that factor each other
 *
 * @date 23/05/30
 * @todo (code review) VND Lift
 * 1. random select 10-vnd paired lift move, paired lift move selects vars from objective and do not factor each other in any constraint
 *
 */

namespace solver
{

    struct opt_solver::imp
    {
        // basic data structures
        var_vector m_vars;
        std::unordered_map<std::string, int> m_var_map;
        constraint_vector m_constraints;
        long double m_object_sum;
        long double m_object_weight;
        long double m_sum_weight;
        long double m_avg_weight;
        const long double weight_gap = 1e3;
        // assignment m_assignment;
        // operation_table m_operation_table;
        int_table m_var_bound_cons, m_cast_cons, m_demand_cons, m_demand_compare_cons, m_other_cons;
        std::unordered_map<int, long double> cast_cons_left;

        // statistics
        int m_lift_step;
        int m_lift_pair_step;
        int m_vnd_lift_step;
        bool find_sat_status = false;
        long double m_best_object_sum = 1e100;
        // assignment m_best_obj_assignment;
        int m_best_unsat_num = INT32_MAX;
        // assignment m_best_unsat_assignment;
        const int random_walk_num = 3;
        int no_improve_unsat; // no improve count if not find sat
        int no_improve_sat;   // no improve count if find sat
        int m_restart;
        int_table m_vars_in_obj;

        // status
        bool is_sat_status;
        int_table m_unsat_constraints;
        int_table m_unbounded_constraints;

        // control
        int m_step;
        const int max_step = INT32_MAX;
        const int BMS_VAL = 1000;
        const int VND_BMS_VAL = 100;
        const int DOUBLE_BMS_VAL = 50;

        // switch
        const bool enable_bound_constraints = true;
        bool enable_initialize_greedy = false;
        const bool enable_tabu = true;
        const int tabu_const = 3, tabu_rand = 10;
        const bool enable_lift_move = true;
        const bool enable_paired_lift_move = false;
        const bool enable_vnd_lift_move = false;
        const bool enable_all_lift_move_pool = false;

        // VND
        const bool enable_vnd_search = false;
        const bool enable_vnd_two_level = false;
        // gradient_vector                                 m_gradient_vector;
        int gradient_step;
        int m_vnd_step;
        int m_vnd_step_two_level;
        double learning_rate = 1;
        const double laerning_rate_decay = 0.95;

        // time
        bool enable_cutoff = false;
        long double m_cutoff;
        std::chrono::steady_clock::time_point m_start_time;
        bool enable_stepoff = false;
        int m_stepoff;

        // trace
        std::ofstream tout;

        imp() : tout("/Users/zhonghanwang/Desktop/multilinear_solver/ls.trace")
        {
            TRACE(tout << "begin\n";);
        }

        // functions
        /**
         * @brief Register a new var in sls solver
         *
         * @param str name of var
         * @param _coeff coeffcient of this var in objective
         * @param lower has lower bound or not
         * @param upper has upper bound or not
         * @param lb lower bound
         * @param ub upper bound
         * @return int var's index in sls solver
         */
        int register_var(std::string str, long double _coeff, bool lower, bool upper, long double lb, long double ub)
        {
            int index = m_vars.size();
            m_vars.push_back(var(index, str, _coeff, lower, upper, lb, ub));
            m_var_map[str] = index;
            if (_coeff != 0)
            {
                m_vars_in_obj.insert(index);
            }
            return index;
        }

        /**
         * @brief Register a new constraint in sls solver
         */
        void register_constraint(polynomial const &poly, constraint_kind _kind, std::string m_name, bool lower, bool upper, long double lb, long double ub)
        {
            int index = m_constraints.size();
            m_constraints.push_back(new constraint(index, m_name, _kind, poly, lower, upper, lb, ub));
            switch (_kind)
            {
            case constraint_kind::CAST:
                m_cast_cons.insert(index);
                break;
            case constraint_kind::DEMAND:
                m_demand_cons.insert(index);
                break;
            case constraint_kind::DEMAND_COMPARE:
                m_demand_compare_cons.insert(index);
                break;
            case constraint_kind::BOUND:
                m_var_bound_cons.insert(index);
                break;
            case constraint_kind::OTHER:
                m_other_cons.insert(index);
                break;
            default:
                break;
            }
        }



        // assignment m_temp_assignment;


        // Select vars meeting two prerequisites
        // 1. appear in objective
        // 2. is bounded by constraint not by its own bound
        int_table m_vars_in_obj_without_bound; // TODO: maintain


        std::unordered_map<int, int> m_var_large_threshold, m_var_small_threshold;



        /*
            3 x1
            [ -3 x2 * x3 ]
        */
        // std::ostream &display_first_monomial(std::ostream &out, monomial const &mono) const
        // {
        //     if (mono.m_coeff == 0 || mono.size() == 0)
        //     {
        //         return out;
        //     }
        //     if (mono.size() == 1)
        //     {
        //         int var_idx = *(mono.m_vars.begin());
        //         out << " " << mono.m_coeff << " " << m_vars[var_idx].m_name;
        //     }
        //     else
        //     {
        //         int index = 0;
        //         out << mono.m_coeff << " ";
        //         for (int var_idx : mono.m_vars)
        //         {
        //             out << m_vars[var_idx].m_name;
        //             if (index != mono.m_vars.size() - 1)
        //             {
        //                 out << " * ";
        //             }
        //             index++;
        //         }
        //     }
        //     return out;
        // }
    };

    opt_solver::opt_solver()
    {
        m_imp = new imp();
    }


    int opt_solver::register_var(std::string str, long double _coeff, bool lower, bool upper, long double lb, long double ub)
    {
        return m_imp->register_var(str, _coeff, lower, upper, lb, ub);
    }

    void opt_solver::register_constraint(polynomial const &poly, constraint_kind _kind, std::string _name, bool lower, bool upper, long double lb, long double ub)
    {
        m_imp->register_constraint(poly, _kind, _name, lower, upper, lb, ub);
    }


    const var_vector& opt_solver::get_vars() const 
    {
        return m_imp->m_vars;
    }

    const constraint_vector& opt_solver::get_constraints() const 
    {
        return m_imp->m_constraints;
    }
};