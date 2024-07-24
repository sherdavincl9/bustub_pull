//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/include/index/index_iterator.h
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
/**
 * index_iterator.h
 * For range scan of b+ tree
 */
#pragma once
#include "storage/page/b_plus_tree_leaf_page.h"

namespace bustub {

#define INDEXITERATOR_TYPE IndexIterator<KeyType, ValueType, KeyComparator>

INDEX_TEMPLATE_ARGUMENTS
class IndexIterator {
 public:
  // you may define your own constructor based on your member variables
  IndexIterator();
  IndexIterator(Page *page, int index_in_leaf, BufferPoolManager *bpm);
  ~IndexIterator();  // NOLINT

  auto IsEnd() -> bool;

  auto operator*() -> const MappingType &;

  auto operator++() -> IndexIterator &;

  auto operator==(const IndexIterator &itr) const -> bool {
    return this->page_id_ == itr.page_id_ && this->index_in_leaf_ == itr.index_in_leaf_;
  }

  auto operator!=(const IndexIterator &itr) const -> bool {
    return this->page_id_ != itr.page_id_ || this->index_in_leaf_ != itr.index_in_leaf_;
  }

  auto operator=(const IndexIterator &other) -> IndexIterator & = delete;
  auto operator=(IndexIterator &&other) noexcept -> IndexIterator &;

 private:
  // add your own private member variables here
  page_id_t page_id_ = INVALID_PAGE_ID;
  Page *page_ = nullptr;
  B_PLUS_TREE_LEAF_PAGE_TYPE *leaf_page_ = nullptr;
  int index_in_leaf_ = 0;
  BufferPoolManager *buffer_pool_manager_ = nullptr;
};

}  // namespace bustub
