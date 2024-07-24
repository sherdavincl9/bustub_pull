/**
 * index_iterator.cpp
 */
#include <cassert>

#include "storage/index/index_iterator.h"

namespace bustub {

/*
 * NOTE: you can change the destructor/constructor method here
 * set your own input parameters
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator() = default;

INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator(Page *page, int index_in_leaf, BufferPoolManager *bpm)
    : page_(page), index_in_leaf_(index_in_leaf), buffer_pool_manager_(bpm) {
  if (page != nullptr) {
    leaf_page_ = reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(page->GetData());
    page_id_ = leaf_page_->GetPageId();
  }
}

INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::~IndexIterator() {
  if (page_ != nullptr) {
    page_->RUnlatch();
    buffer_pool_manager_->UnpinPage(page_id_, false);
  }
}  // NOLINT

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::IsEnd() -> bool { return page_ == nullptr; }

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator*() -> const MappingType & {
  if (index_in_leaf_ >= leaf_page_->GetSize()) {
    throw std::logic_error("check operator++, index error.");
  }
  return leaf_page_->GetKeyValueAt(index_in_leaf_);
}

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator++() -> INDEXITERATOR_TYPE & {
  if (page_id_ == INVALID_PAGE_ID) {
    return *this;
  }
  if (index_in_leaf_ < leaf_page_->GetSize() - 1) {
    index_in_leaf_++;
  } else {  // need to jump to next leaf page.
    index_in_leaf_ = 0;
    page_id_t pre_page_id = page_id_;
    page_id_ = leaf_page_->GetNextPageId();
    page_->RUnlatch();
    if (page_id_ == INVALID_PAGE_ID) {
      page_ = nullptr;
      leaf_page_ = nullptr;
    } else {
      page_ = buffer_pool_manager_->FetchPage(page_id_);
      page_->RLatch();
      leaf_page_ = reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(page_->GetData());
    }
    buffer_pool_manager_->UnpinPage(pre_page_id, false);
  }
  return *this;
}

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator=(IndexIterator &&other) noexcept -> IndexIterator & {
  std::swap(page_id_, other.page_id_);
  std::swap(page_, other.page_);
  std::swap(leaf_page_, other.leaf_page_);
  std::swap(index_in_leaf_, other.index_in_leaf_);
  std::swap(buffer_pool_manager_, other.buffer_pool_manager_);
  return *this;
}

template class IndexIterator<GenericKey<4>, RID, GenericComparator<4>>;

template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>>;

template class IndexIterator<GenericKey<16>, RID, GenericComparator<16>>;

template class IndexIterator<GenericKey<32>, RID, GenericComparator<32>>;

template class IndexIterator<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
