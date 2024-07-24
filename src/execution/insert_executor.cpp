//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// insert_executor.cpp
//
// Identification: src/execution/insert_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <memory>

#include "execution/executors/insert_executor.h"

namespace bustub {

InsertExecutor::InsertExecutor(ExecutorContext *exec_ctx, const InsertPlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void InsertExecutor::Init() {
  child_executor_->Init();
  TableInfo *table_info = exec_ctx_->GetCatalog()->GetTable(plan_->TableOid());
  try {
    exec_ctx_->GetLockManager()->LockTable(exec_ctx_->GetTransaction(), LockManager::LockMode::INTENTION_EXCLUSIVE,
                                           table_info->oid_);
  } catch (TransactionAbortException &e) {
    exec_ctx_->GetTransaction()->SetState(TransactionState::ABORTED);
    throw ExecutionException("fail to lock IX on table");
  }
}

auto InsertExecutor::Next([[maybe_unused]] Tuple *tuple, RID *rid) -> bool {
  // 插入完毕返回 false
  if (has_inserted_) {
    return false;
  }
  has_inserted_ = true;

  // 获取待插入的表信息及其索引列表
  TableInfo *table_info = exec_ctx_->GetCatalog()->GetTable(plan_->TableOid());
  std::vector<IndexInfo *> index_info = exec_ctx_->GetCatalog()->GetTableIndexes(table_info->name_);

  // 从子执行器 Values 中逐个获取元组并插入到表中，同时更新所有的索引
  int insert_count = 0;
  while (child_executor_->Next(tuple, rid)) {
    try {
      exec_ctx_->GetLockManager()->LockRow(exec_ctx_->GetTransaction(), LockManager::LockMode::EXCLUSIVE,
                                           table_info->oid_, *rid);
    } catch (TransactionAbortException &e) {
      exec_ctx_->GetTransaction()->SetState(TransactionState::ABORTED);
      throw ExecutionException("fail to lock X on row");
    }
    table_info->table_->InsertTuple(*tuple, rid, exec_ctx_->GetTransaction());
    for (const auto &index : index_info) {
      // 根据索引的模式从数据元组中构造索引元组，并插入到索引中
      Tuple key_tuple = tuple->KeyFromTuple(child_executor_->GetOutputSchema(), index->key_schema_,
                                            index->index_->GetMetadata()->GetKeyAttrs());
      index->index_->InsertEntry(key_tuple, *rid, exec_ctx_->GetTransaction());
      IndexWriteRecord idx_write_record(*rid, table_info->oid_, WType::INSERT, *tuple, index->index_oid_,
                                        exec_ctx_->GetCatalog());
      exec_ctx_->GetTransaction()->GetIndexWriteSet()->push_back(idx_write_record);
    }
    insert_count++;
  }

  // 这里的 tuple 不再对应实际的数据行，而是用来存储插入操作的影响行数
  std::vector<Value> result{Value(INTEGER, insert_count)};
  *tuple = Tuple(result, &plan_->OutputSchema());

  return true;
}

}  // namespace bustub
