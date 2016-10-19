#ifndef TACO_ITERATION_SCHEDULE_H
#define TACO_ITERATION_SCHEDULE_H

#include <memory>
#include <vector>

namespace taco {
class Var;

namespace internal {
class Tensor;
class TensorPath;
class MergeRule;

/// An iteration schedule is a two dimensional ordering of index variables,
/// tensor paths that describe how to reach non-zero index variable values
/// through tensor indices, and a constraint on each index variable that tells
/// us how to merge tensor index values.
class IterationSchedule {
public:
  IterationSchedule();

  /// Creates an iteration schedule for a tensor with a defined expression.
  static IterationSchedule make(const Tensor&);

  /// Returns a two dimensional ordering of index variables. The first (x)
  /// dimension corresponds to nested loops and the second (y) dimension
  /// correspond to sequenced loops.
  const std::vector<std::vector<taco::Var>>& getIndexVariables() const;

  /// Returns the merge rule of the given var.
  const MergeRule& getMergeRule(const Var&) const;

  /// Returns the tensor paths of the iteration schedule
  const std::vector<TensorPath>& getTensorPaths() const;

  private:
  struct Content;
  std::shared_ptr<Content> content;
};

std::ostream& operator<<(std::ostream&, const IterationSchedule&);

}}
#endif