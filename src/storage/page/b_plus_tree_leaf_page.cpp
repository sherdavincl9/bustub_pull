//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/page/b_plus_tree_leaf_page.cpp
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <sstream>

#include "common/exception.h"
#include "common/rid.h"
#include "storage/page/b_plus_tree_leaf_page.h"

namespace bustub {

/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/

/**
 * Init method after creating a new leaf page
 * Including set page type, set current size to zero, set page id/parent id, set
 * next page id and set max size
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::Init(page_id_t page_id, page_id_t parent_id, int max_size) {
  this->SetPageType(IndexPageType::LEAF_PAGE);
  this->SetSize(0);
  this->SetPageId(page_id);
  this->SetParentPageId(parent_id);
  this->SetMaxSize(max_size);
  SetNextPageId(INVALID_PAGE_ID);
}

/**
 * Helper methods to set/get next page id
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::GetNextPageId() const -> page_id_t { return next_page_id_; }

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::SetNextPageId(page_id_t next_page_id) { this->next_page_id_ = next_page_id; }

/*
 * Helper method to find and return the key associated with input "index"(a.k.a
 * array offset)
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::KeyAt(int index) const -> KeyType {
  // replace with your own code
  KeyType key{array_[index].first};
  return key;
}

// My function implementation.
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::SetKeyValueAt(int index, const KeyType &key, const ValueType &value) {
  array_[index].first = key;
  array_[index].second = value;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::InsertByKey(const KeyType &key, const ValueType &value, const KeyComparator &compara) {
  int size = GetSize();
  if (size >= GetMaxSize()) {
    throw std::logic_error("This leaf node is full before insertion");
  }
  int index = 0;  // find the location for insertion.
  while (index < size && compara(array_[index].first, key) < 0) {
    index++;
  }
  for (int i = size; i > index; i--) {
    array_[i] = array_[i - 1];
  }
  IncreaseSize(1);
  SetKeyValueAt(index, key, value);
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::RemoveByKey(const KeyType &key, const KeyComparator &compara) {
  int size = GetSize();
  int index = 0;  // find the location for insertion.
  while (index < size && compara(array_[index].first, key) != 0) {
    index++;
  }
  if (index == size) {
    return;
  }
  for (int i = index; i < size - 1; i++) {
    array_[i] = array_[i + 1];
  }
  SetSize(size - 1);
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::DataMove(B_PLUS_TREE_LEAF_PAGE_TYPE *new_page, int count) {
  int size = GetSize();
  int start = size - count;
  for (int i = start, j = 0; i < size; i++, j++) {
    new_page->SetKeyValueAt(j, array_[i].first, array_[i].second);
  }
  new_page->IncreaseSize(count);
  SetSize(size - count);  // logical deletion.
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::LowerBound(const KeyType &key, const KeyComparator &compara) -> int {
  int index = 0;
  int size = GetSize();
  while (index < size && compara(array_[index].first, key) < 0) {
    index++;
  }
  return index;
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::GetKeyValueAt(int index) const -> const MappingType & {
  int size = GetSize();
  if (index >= size) {
    throw std::logic_error("This index out of range.");
  }
  return array_[index];
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::ValueAt(int index) const -> ValueType {
  ValueType value{array_[index].second};
  return value;
}

template class BPlusTreeLeafPage<GenericKey<4>, RID, GenericComparator<4>>;
template class BPlusTreeLeafPage<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTreeLeafPage<GenericKey<16>, RID, GenericComparator<16>>;
template class BPlusTreeLeafPage<GenericKey<32>, RID, GenericComparator<32>>;
template class BPlusTreeLeafPage<GenericKey<64>, RID, GenericComparator<64>>;
}  // namespace bustub
