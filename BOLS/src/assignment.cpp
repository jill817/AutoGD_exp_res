#include "assignment.hpp"

class allocateVar;

void OperatorPool::clear() {
    if (JUDGE) assert(_vars.size() == _vals.size());
    _vars.clear();
    _vals.clear();
    _operatorSet.clear();
    _size = 0;
}

long long OperatorPool::size() {
    if (JUDGE) assert(_vars.size() == _vals.size());
    return _vars.size();
}

void OperatorPool::push(allocateVar var, long long val) {
    if (JUDGE) assert(_vars.size() == _vals.size());
    if (JUDGE) assert(_vars.size() == _size);

    //if (!haveOperator(var, val)) {
        _vars.push_back(var);
        _vals.push_back(val);

//        addOperator(var, val);
        _size++;
    //}
}

bool OperatorPool::empty() const {
    if (JUDGE) assert(_vars.size() == _vals.size());
    return _vars.empty();
}

// bool OperatorPool::haveOperator(const allocateVar& var, const long long& val) {
//     if (_operatorSet.count(var) != 0 && _operatorSet[var].count(val) != 0) return true;
//     return false;
// }

// void OperatorPool::addOperator(const allocateVar& var, const long long& val) {
//     if (JUDGE) assert(!haveOperator(var, val));

//     if (_operatorSet.count(var) == 0) {
//         _operatorSet.insert(std::make_pair(var, Set<Int>()));
//     }
//     _operatorSet[var].insert(val);
// }


allocateVar OperatorPool::varAt(long long index) const {
    if (JUDGE) assert(index < _vars.size());
    return _vars.at(index);
}

long long OperatorPool::valAt(long long index) const {
    if (JUDGE) assert(index < _vals.size());
    return _vals.at(index);
}

void OperatorPool::removeOpAt(long long index) {
    _size--;
    _vars[index] = _vars[_size];
    _vals[index] = _vals[_size];
}

void OperatorPool::printOperators() const {

}
