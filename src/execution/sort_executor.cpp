#include "execution/executors/sort_executor.h"

namespace bustub {

SortExecutor::SortExecutor(ExecutorContext *exec_ctx, const SortPlanNode *plan,
                           std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void SortExecutor::Init() {
  assert(child_executor_ != nullptr && "Child executor is null");
  child_executor_->Init();
  Tuple tuple;
  RID rid;
  while (child_executor_->Next(&tuple, &rid)) {
    tuples_.emplace_back(tuple);
  }
  std::sort(tuples_.begin(), tuples_.end(), [this](const Tuple &a, const Tuple &b) -> bool {
    assert(plan_ != nullptr && "Plan is null");
    for (auto const &order_by : plan_->GetOrderBy()) {
      const auto order_type = order_by.first;
      // 使用Evaluate获取值
      AbstractExpressionRef expr = order_by.second;
      assert(expr != nullptr && "Expression is null");
      Value v1 = expr->Evaluate(&a, GetOutputSchema());
      Value v2 = expr->Evaluate(&b, GetOutputSchema());
      if (v1.CompareEquals(v2) == CmpBool::CmpTrue) {
        continue;
      }
      // 如果是升序（ASC 或 DEFAULT），比较 v1 是否小于 v2（CompareLessThan）
      if (order_type == OrderByType::ASC || order_type == OrderByType::DEFAULT) {
        return v1.CompareLessThan(v2) == CmpBool::CmpTrue;
      }
      // 如果是降序（DESC），比较 v1 是否大于 v2（CompareGreaterThan）
      return v1.CompareGreaterThan(v2) == CmpBool::CmpTrue;
    }
    // 两个元组所有键都相等
    return false;
  });
  iter_ = tuples_.begin();
}

auto SortExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  if (iter_ != tuples_.end()) {
    *tuple = *iter_;
    ++iter_;
    return true;
  }
  return false;
}

}  // namespace bustub
