#include <array>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <memory>
#include <string>
#include <cstring>

#include "proof/fra/fra.h"
#include "InterpolatingSolver.h"
// #include "AvyMain.h"

using namespace std;
using namespace avy;

InterpolatingSolver::InterpolatingSolver(int nr_variables): solver(2, nr_variables, false), combined_aig(nullptr), nr_variables(nr_variables) {
  solver.reserve(nr_variables + 1);
}

InterpolatingSolver::~InterpolatingSolver() {
  if (combined_aig != nullptr) {
    Aig_ManStop(combined_aig);
  }
}

void InterpolatingSolver::resetSolver(int nr_variables) {
  assert(nr_variables > 0);
  this->nr_variables = nr_variables;
  solver.reset(2, nr_variables + 1);
  solver.reserve(nr_variables + 1);
}

void InterpolatingSolver::resetSolver() {
  resetSolver(nr_variables);
}

void InterpolatingSolver::reserve(int nr_variables) {
  this->nr_variables = max(this->nr_variables, nr_variables);
  solver.reserve(nr_variables + 1);
}

void InterpolatingSolver::addFormula(clauseSet& first_part, clauseSet& second_part) {
  solver.markPartition(1);
  for (auto& clause: first_part) {
    if (clause.size() == 1) {
      solver.addUnit(clause.front());
    } else {
      solver.addClause(clause);
    }
  }
  solver.markPartition(2);
  for (auto& clause: second_part) {
    if (clause.size() == 1) {
      solver.addUnit(clause.front());
    } else {
      solver.addClause(clause);
    }
  }
}

bool InterpolatingSolver::addClause(vector<int>& literals, int part) {
  if (part) {
    solver.markPartition(part);
  }
  return solver.addClause(literals);
}

bool InterpolatingSolver::solve(const vector<int>& assumptions) {
  return solver.solve(assumptions);
}

vector<int> InterpolatingSolver::getConflict() {
  int *conflict;
  int size = solver.core(&conflict);
  vector<int> conflict_vector(conflict, conflict + size);
  return conflict_vector;
}

boost::tribool InterpolatingSolver::getVarVal(int variable) {
  return solver.getVarVal(variable);
}

boost::tribool InterpolatingSolver::getInterpolant(int variable, vector<int>& assumptions, vector<int>& shared_variables, int budget)
{
  boost::tribool result = solver.solve(assumptions, budget);
  if (!result) {
    output_variables.push_back(variable);
    Aig_Man_t* aig_interpolant = solver.getInterpolant(shared_variables, std::max<int>(1, shared_variables.size()));
    sum_AIG_sizes += avy::abc::Aig_ManObjNum(aig_interpolant);
    
    if (combined_aig == nullptr) {
      combined_aig = aig_interpolant;
    } else {
      combined_aig = Aig_ManCombine(combined_aig, aig_interpolant);
      Aig_ManStop(aig_interpolant);

      if (avy::abc::Aig_ManObjNum(combined_aig) > limit) {
        Dar_LibStart();
        combined_aig = Dar_ManRewriteDefault(combined_aig);
        limit *= factor;
        Dar_LibStop();
      }
    }
  }
  return result;
}

avy::abc::Aig_Man_t * InterpolatingSolver::getCircuit(vector<int>& input_variables, bool compress) {
  if (combined_aig != nullptr) {
    assert( Aig_ManCiNum(combined_aig) <= input_variables.size() || Aig_ManCiNum(combined_aig) == 1);
    while ( Aig_ManCiNum(combined_aig) < input_variables.size() ) {
      // TODO: Need to check that the input IDs are consistent!
      Aig_ObjCreateCi( combined_aig );
    }
    if (compress) {
      Dar_LibStart();
      std::cerr << "Compressing final AIG." << std::endl;
      std::cerr << "Size before compression: " << Aig_ManObjNum(combined_aig) << std::endl;
      combined_aig = Dar_ManCompress2(combined_aig, 1, 0, 1, 0, 0);
      std::cerr << "Size after compression: " << Aig_ManObjNum(combined_aig) << std::endl;
      if (Aig_ManObjNum(combined_aig) < 100000) {
        avy::abc::Fra_Par_t pars, *p_pars = &pars;
        Fra_ParamsDefault(p_pars);
        std::cerr << "Fraiging AIG." << std::endl;
        combined_aig = Fra_FraigPerform(combined_aig, p_pars);
        std::cerr << "Size after fraiging: " << Aig_ManObjNum(combined_aig) << std::endl;
        std::cerr << "Compressing AIG." << std::endl;
        combined_aig = Dar_ManCompress2(combined_aig, 1, 0, 1, 0, 0);
        std::cerr << "Size after compression: " << Aig_ManObjNum(combined_aig) << std::endl;
      }
      Dar_LibStop();
    }
    std::cerr << "AIG nodes per variable: " << (double)(Aig_ManObjNum(combined_aig)) / (double)output_variables.size() << std::endl;
  }
  return combined_aig;
}

vector<tuple<vector<int>,int>> InterpolatingSolver::getDefinitions(vector<int>& input_variables, vector<int>& output_variables, bool compress, int auxiliary_start) {
  avy::abc::Aig_Man_t * circuit = getCircuit(input_variables, compress);

  vector<tuple<vector<int>,int>> definitions;

  if (circuit != nullptr) {
    // Make sure number of inputs and outputs matches expectations
    assert(Aig_ManCiNum(circuit) == input_variables.size());
    assert(Aig_ManCoNum(circuit) == output_variables.size());
    
    Aig_Obj_t *pObj, *pConst1 = nullptr;
    int i;

    // check if constant is used
    Aig_ManForEachCo(circuit, pObj, i)
      if (Aig_ObjIsConst1(Aig_ObjFanin0(pObj)))
        pConst1 = Aig_ManConst1(circuit);
    Aig_ManConst1(circuit)->iData = ++auxiliary_start;

    // collect nodes in the DFS order
    Vec_Ptr_t * vNodes;
    vNodes = Aig_ManDfs(circuit, 1);

    // assign new variable ids to
    Vec_PtrForEachEntry(Aig_Obj_t*, vNodes, pObj, i) {
      pObj->iData = ++auxiliary_start;
    }
    // ith input/output gets CioId i.
    Aig_ManSetCioIds(circuit);
    // Add definition for constant true.
    if (pConst1) {
      vector<int> empty;
      definitions.push_back(std::make_tuple(empty, pConst1->iData));
    }
    // Add definitions for internal nodes.
    Vec_PtrForEachEntry(Aig_Obj_t *, vNodes, pObj, i) {
      int first_input, second_input, output;
      if (Aig_ObjIsCi(Aig_ObjFanin0(pObj))) {
        first_input = input_variables[Aig_ObjCioId(Aig_ObjFanin0(pObj))];
      } else {
        first_input = Aig_ObjFanin0(pObj)->iData;
      }
      if (Aig_ObjIsCi(Aig_ObjFanin1(pObj))) {
        second_input = input_variables[Aig_ObjCioId(Aig_ObjFanin1(pObj))];
      } else {
        second_input = Aig_ObjFanin1(pObj)->iData;
      }
      output = pObj->iData;
      vector<int> input_literals;
      if (!Aig_ObjFaninC0(pObj)) {
        input_literals.push_back(first_input);
      } else {
        input_literals.push_back(-first_input);
      }
      if (!Aig_ObjFaninC1(pObj)) {
        input_literals.push_back(second_input);
      } else {
        input_literals.push_back(-second_input);
      }
      definitions.push_back(std::make_tuple(input_literals, output));
    }
    // Add definitions for output nodes.
    Aig_ManForEachCo(circuit, pObj, i) {
      int input, output;
      if (Aig_ObjIsCi(Aig_ObjFanin0(pObj))) {
        input = input_variables[Aig_ObjCioId(Aig_ObjFanin0(pObj))];
      } else {
        input = Aig_ObjFanin0(pObj)->iData;
      }
      output = output_variables[Aig_ObjCioId(pObj)];
      vector<int> input_literals;
      if (!Aig_ObjFaninC0(pObj)) {
        input_literals.push_back(input);
      } else {
        input_literals.push_back(-input);
      }
      definitions.push_back(std::make_tuple(input_literals, output));
    }
    Aig_ManCleanCioIds(circuit);
    Vec_PtrFree(vNodes);
  }
  return definitions;
}

tuple<vector<tuple<vector<int>,int>>, vector<int>> InterpolatingSolver::getDefinition(const vector<int>& input_variables, int output_variable, bool compress, int auxiliary_start) {
  assert(solver.isSolved());
  Aig_Man_t* circuit = solver.getInterpolant(input_variables, std::max<int>(1, input_variables.size()));
  vector<tuple<vector<int>,int>> definitions;
  vector<bool> input_variables_used_characteristic(input_variables.size());
  std::fill(input_variables_used_characteristic.begin(), input_variables_used_characteristic.end(), false);

  if (circuit != nullptr) {
    // Make sure number of inputs matches expectations
    assert((Aig_ManCiNum(circuit) == input_variables.size()) || ((Aig_ManCiNum(circuit) == 1) && (input_variables.size() == 0)));

    if (compress) {
      Dar_LibStart();
      std::cerr << "Size before compression: " << Aig_ManObjNum(circuit) << std::endl;
      circuit = Dar_ManCompress2(circuit, 1, 0, 1, 0, 0);
      //std::cerr << "Size after compression: " << Aig_ManObjNum(circuit) << std::endl;
      if (Aig_ManObjNum(circuit) < 100000) {
        avy::abc::Fra_Par_t pars, *p_pars = &pars;
        Fra_ParamsDefault(p_pars);
        //std::cerr << "Fraiging AIG." << std::endl;
        circuit = Fra_FraigPerform(circuit, p_pars);
        //std::cerr << "Size after fraiging: " << Aig_ManObjNum(circuit) << std::endl;
        //std::cerr << "Compressing AIG." << std::endl;
        circuit = Dar_ManCompress2(circuit, 1, 0, 1, 0, 0);
        std::cerr << "Size after compression: " << Aig_ManObjNum(circuit) << std::endl;
      }
      Dar_LibStop();
    }
    
    Aig_Obj_t *pObj, *pConst1 = nullptr;
    int i;

    // check if constant is used
    Aig_ManForEachCo(circuit, pObj, i)
      if (Aig_ObjIsConst1(Aig_ObjFanin0(pObj)))
        pConst1 = Aig_ManConst1(circuit);
    Aig_ManConst1(circuit)->iData = ++auxiliary_start;

    // collect nodes in the DFS order
    Vec_Ptr_t * vNodes;
    vNodes = Aig_ManDfs(circuit, 1);

    // assign new variable ids to
    Vec_PtrForEachEntry(Aig_Obj_t*, vNodes, pObj, i) {
      pObj->iData = ++auxiliary_start;
    }
    // ith input/output gets CioId i.
    Aig_ManSetCioIds(circuit);
    // Add definition for constant true.
    if (pConst1) {
      vector<int> empty;
      definitions.push_back(std::make_tuple(empty, pConst1->iData));
    }
    // Add definitions for internal nodes.
    Vec_PtrForEachEntry(Aig_Obj_t *, vNodes, pObj, i) {
      int first_input, second_input, output;
      if (Aig_ObjIsCi(Aig_ObjFanin0(pObj))) {
        auto index = Aig_ObjCioId(Aig_ObjFanin0(pObj));
        input_variables_used_characteristic[index] = true;
        first_input = input_variables[index];
      } else {
        first_input = Aig_ObjFanin0(pObj)->iData;
      }
      if (Aig_ObjIsCi(Aig_ObjFanin1(pObj))) {
        auto index = Aig_ObjCioId(Aig_ObjFanin1(pObj));
        input_variables_used_characteristic[index] = true;
        second_input = input_variables[index];
      } else {
        second_input = Aig_ObjFanin1(pObj)->iData;
      }
      output = pObj->iData;
      vector<int> input_literals;
      if (!Aig_ObjFaninC0(pObj)) {
        input_literals.push_back(first_input);
      } else {
        input_literals.push_back(-first_input);
      }
      if (!Aig_ObjFaninC1(pObj)) {
        input_literals.push_back(second_input);
      } else {
        input_literals.push_back(-second_input);
      }
      definitions.push_back(std::make_tuple(input_literals, output));
    }
    // Add definitions for output nodes.
    Aig_ManForEachCo(circuit, pObj, i) {
      int input, output;
      if (Aig_ObjIsCi(Aig_ObjFanin0(pObj))) {
        auto index = Aig_ObjCioId(Aig_ObjFanin0(pObj));
        input_variables_used_characteristic[index] = true;
        input = input_variables[index];
      } else {
        input = Aig_ObjFanin0(pObj)->iData;
      }
      output = output_variable;
      vector<int> input_literals;
      if (!Aig_ObjFaninC0(pObj)) {
        input_literals.push_back(input);
      } else {
        input_literals.push_back(-input);
      }
      definitions.push_back(std::make_tuple(input_literals, output));
    }
    Aig_ManCleanCioIds(circuit);
    Vec_PtrFree(vNodes);
  }
  vector<int> input_variables_used;
  for (int i = 0; i < input_variables_used_characteristic.size(); i++) {
    if (input_variables_used_characteristic[i]) {
      input_variables_used.push_back(input_variables[i]);
    }
  }
  return std::make_tuple(definitions, input_variables_used);
}

void InterpolatingSolver::interrupt() {
  solver.get()->interrupt();
}
