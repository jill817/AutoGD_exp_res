#ifndef SLS_SOLVER_MODE_H
#define SLS_SOLVER_MODE_H

namespace solver {

enum ModelMode {
    ZeroObj,   // 0obj
    HeatObj,   // heatobj
    GivenQueryObj,   // givenqueryobj
    RandQueryObj, // randqueryobj
    LastQueryObj, // lastqueryobj
    MaxQueryObj // maxqueryobj
};

} // namespace solver

#endif // SLS_SOLVER_MODE_H