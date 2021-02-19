#ifndef _INTERPOLATING_SOLVER_H
#define _INTERPOLATING_SOLVER_H

#include <vector>
#include <string>
#include <tuple>

#include "AigUtils.h"
#include "ItpMinisat.h"

using namespace std;

typedef vector< vector<int> > clauseSet;

class InterpolatingSolver {

public:
  InterpolatingSolver(int nr_variables);
  virtual ~InterpolatingSolver();
  void resetSolver(int nr_variables);
  void resetSolver();
  void reserve(int nr_variables);
  void addFormula(clauseSet& first_part, clauseSet& second_part);
  bool addClause(vector<int>& literals, int part=0);
  bool solve(const vector<int>& assumptions=vector<int>());
  vector<int> getConflict();
  boost::tribool getVarVal(int variable);
  boost::tribool getInterpolant(int variable, vector<int>& assumptions, vector<int>& shared_variables, int budget);
  avy::abc::Aig_Man_t * getCircuit(vector<int>& input_variables, bool compress);
  vector<tuple<vector<int>,int>> getDefinitions(vector<int>& input_variables, vector<int>& output_variables, bool compress, int auxiliary_start);
  tuple<vector<tuple<vector<int>,int>>, vector<int>> getDefinition(vector<int>& input_variables, int output_variable, bool compress, int auxiliary_start);
  void interrupt();

private:
  avy::ItpMinisat solver;
  avy::abc::Aig_Man_t * combined_aig;
  vector<int> output_variables;
  int nr_variables;
  int sum_AIG_sizes = 0;
  int limit = 100000;
  float factor = 1.05;

};

#endif //_INTERPOLATING_SOLVER_H