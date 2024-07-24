#include <string>

#include "common/exception.h"
#include "common/logger.h"
#include "common/rid.h"
#include "storage/index/b_plus_tree.h"
#include "storage/page/header_page.h"

namespace bustub {
INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, BufferPoolManager *buffer_pool_manager, const KeyComparator &comparator,
                          int leaf_max_size, int internal_max_size)
    : index_name_(std::move(name)),
      root_page_id_(INVALID_PAGE_ID),
      buffer_pool_manager_(buffer_pool_manager),
      comparator_(comparator),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size) {}

/*
 * Helper function to decide whether current b+tree is empty
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsEmpty() const -> bool {
  // if (root_page_id_ == INVALID_PAGE_ID) {
  //   return true;
  // }
  // Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
  // auto tree_page = reinterpret_cast<BPlusTreePage *>(page->GetData());
  // return tree_page->IsLeafPage() && tree_page->GetSize() == 0;
  return root_page_id_ == INVALID_PAGE_ID;
}

/*****************************************************************************
 * My function
 *****************************************************************************/
// This funcction is to check whether current page is safe.
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsPageSafe(BPlusTreePage *tree_page, Operation op) -> bool {
  if (op == Operation::Read) {
    return true;
  }
  if (op == Operation::Insert) {
    if (tree_page->IsLeafPage()) {
      return tree_page->GetSize() < tree_page->GetMaxSize() - 1;
    }
    return tree_page->GetSize() < tree_page->GetMaxSize();
  }
  if (op == Operation::Remove) {
    if (tree_page->IsRootPage()) {
      if (tree_page->IsLeafPage()) {
        return tree_page->GetSize() > 1;
      }
      return tree_page->GetSize() > 2;
    }
    return tree_page->GetSize() > tree_page->GetMinSize();
  }
  return false;
}

// This function is to release all write latch in ancestor node
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ReleaseWLatches(Transaction *transaction) {
  if (transaction == nullptr) {
    return;
  }
  auto page_set = transaction->GetPageSet();
  while (!page_set->empty()) {
    Page *page = page_set->front();
    page_set->pop_front();
    if (page == nullptr) {
      root_latch_.WUnlock();
    } else {
      page->WUnlatch();
      buffer_pool_manager_->UnpinPage(page->GetPageId(), true);
    }
  }
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetPageFromTransaction(page_id_t page_id, Transaction *transaction) -> Page * {
  assert(transaction != nullptr);
  auto page_set = transaction->GetPageSet();
  for (auto it = page_set->rbegin(); it != page_set->rend(); ++it) {
    Page *page = *it;
    if (page != nullptr && page->GetPageId() == page_id) {
      return page;
    }
  }
  throw std::logic_error("Getting non-exist page from transaction.");
  return nullptr;
}

/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetLeafPage(const KeyType &key, Transaction *transaction, Operation op, bool first_pass)
    -> Page * {
  if (transaction == nullptr && op != Operation::Read) {
    throw std::logic_error("Insertion or remove operation must be given a not-null transaction.");
  }
  if (!first_pass) {
    root_latch_.WLock();
    transaction->AddIntoPageSet(nullptr);
  }
  page_id_t cur_page_id = root_page_id_;
  Page *prev_page = nullptr;  // Using for latch operation;

  while (true) {
    Page *page = buffer_pool_manager_->FetchPage(cur_page_id);
    auto tree_page = reinterpret_cast<BPlusTreePage *>(page->GetData());
    // Optimized latch crabbing.
    if (first_pass) {
      if (tree_page->IsLeafPage() && op != Operation::Read) {
        page->WLatch();
        transaction->AddIntoPageSet(page);
      } else {
        page->RLatch();
      }
      if (prev_page != nullptr) {
        prev_page->RUnlatch();
        buffer_pool_manager_->UnpinPage(prev_page->GetPageId(), false);
      } else {
        root_latch_.RUnlock();
      }
    } else {
      assert(op != Operation::Read);
      page->WLatch();
      if (IsPageSafe(tree_page, op)) {
        ReleaseWLatches(transaction);
      }
      transaction->AddIntoPageSet(page);
    }

    if (tree_page->IsLeafPage()) {
      // unsafe leaf for insert or remove.
      if (first_pass && !IsPageSafe(tree_page, op)) {
        ReleaseWLatches(transaction);
        return GetLeafPage(key, transaction, op, false);
      }
      return page;
    }
    auto internal_page = static_cast<InternalPage *>(tree_page);
    cur_page_id = internal_page->ValueAt(internal_page->GetSize() - 1);
    for (int i = 1; i < internal_page->GetSize(); i++) {
      if (comparator_(internal_page->KeyAt(i), key) > 0) {
        cur_page_id = internal_page->ValueAt(i - 1);
        break;
      }
    }
    prev_page = page;
  }
}
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result, Transaction *transaction) -> bool {
  root_latch_.RLock();
  if (IsEmpty()) {
    root_latch_.RUnlock();
    return false;
  }
  bool isfound = false;
  Page *page = GetLeafPage(key, nullptr, Operation::Read);
  auto leaf_page = reinterpret_cast<LeafPage *>(page->GetData());
  for (int i = 0; i < leaf_page->GetSize(); i++) {
    if (comparator_(leaf_page->KeyAt(i), key) == 0) {
      isfound = true;
      result->emplace_back(leaf_page->ValueAt(i));
    }
  }
  page->RUnlatch();
  buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
  return isfound;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value, Transaction *transaction) -> bool {
  root_latch_.RLock();

  // case 1:this B+Tree is empty before insertion.
  if (IsEmpty()) {
    root_latch_.RUnlock();
    root_latch_.WLock();
    if (IsEmpty()) {
      Page *page = buffer_pool_manager_->NewPage(&root_page_id_);
      UpdateRootPageId(1);  // para. = 1 means inserting a new root page.
      auto leaf_page = reinterpret_cast<LeafPage *>(page->GetData());
      leaf_page->Init(root_page_id_, INVALID_PAGE_ID, leaf_max_size_);
      leaf_page->SetKeyValueAt(0, key, value);
      leaf_page->IncreaseSize(1);
      leaf_page->SetNextPageId(INVALID_PAGE_ID);
      buffer_pool_manager_->UnpinPage(root_page_id_, true);
      root_latch_.WUnlock();
      return true;
    }
    root_latch_.WUnlock();
    root_latch_.RLock();
  }

  Page *page = GetLeafPage(key, transaction, Operation::Insert);
  auto leaf_page = reinterpret_cast<LeafPage *>(page->GetData());

  // case 2: Insert duplicate keys. So return false.
  for (int i = 0; i < leaf_page->GetSize(); i++) {
    if (comparator_(leaf_page->KeyAt(i), key) == 0) {
      ReleaseWLatches(transaction);
      // buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
      return false;
    }
  }

  // case 3: Number of key/value pairs AFTER insertion less than max_size for leaf nodes.
  leaf_page->InsertByKey(key, value, comparator_);
  if (leaf_page->GetSize() < leaf_max_size_) {
    ReleaseWLatches(transaction);
    // buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), true);
    return true;
  }
  // case 4: This leaf node is full, so split it;
  page_id_t new_page_id;
  Page *new_page = buffer_pool_manager_->NewPage(&new_page_id);
  auto new_leaf_page = reinterpret_cast<LeafPage *>(new_page->GetData());
  new_leaf_page->Init(new_page_id, leaf_page->GetParentPageId(), leaf_max_size_);
  new_leaf_page->SetNextPageId(leaf_page->GetNextPageId());
  leaf_page->SetNextPageId(new_page_id);
  leaf_page->DataMove(new_leaf_page, (leaf_max_size_ + 1) / 2);  // split data into two parts;

  /* * * key part * * */
  // Handle insertion to parent node, while also handling recursive logic where the parent node may continue to split.
  BPlusTreePage *old_tree_page = leaf_page;
  BPlusTreePage *new_tree_page = new_leaf_page;
  KeyType split_key = new_leaf_page->KeyAt(0);
  while (true) {
    if (old_tree_page->IsRootPage()) {
      Page *new_page = buffer_pool_manager_->NewPage(&root_page_id_);
      auto new_root_page = reinterpret_cast<InternalPage *>(new_page->GetData());
      new_root_page->Init(root_page_id_, INVALID_PAGE_ID, internal_max_size_);
      new_root_page->SetKeyValueAt(0, split_key, old_tree_page->GetPageId());  // First key doesn't used.
      new_root_page->SetKeyValueAt(1, split_key, new_tree_page->GetPageId());
      new_root_page->IncreaseSize(2);
      old_tree_page->SetParentPageId(root_page_id_);
      new_tree_page->SetParentPageId(root_page_id_);
      UpdateRootPageId(0);
      buffer_pool_manager_->UnpinPage(root_page_id_, true);
      break;
    }
    page_id_t parent_page_id = old_tree_page->GetParentPageId();
    Page *parent_page = buffer_pool_manager_->FetchPage(parent_page_id);
    auto parent_internal_page = reinterpret_cast<InternalPage *>(parent_page->GetData());
    parent_internal_page->InsertByKey(split_key, new_tree_page->GetPageId(), comparator_);
    new_tree_page->SetParentPageId(parent_internal_page->GetPageId());
    if (parent_internal_page->GetSize() <= internal_max_size_) {
      buffer_pool_manager_->UnpinPage(parent_page_id, true);
      break;
    }
    // parent node is full.
    page_id_t new_page_id;
    Page *new_page = buffer_pool_manager_->NewPage(&new_page_id);
    auto new_internal_page = reinterpret_cast<InternalPage *>(new_page->GetData());
    new_internal_page->Init(new_page_id, parent_internal_page->GetParentPageId(), internal_max_size_);
    int new_page_size = (internal_max_size_ + 1) / 2;
    size_t start = parent_internal_page->GetSize() - new_page_size;
    for (int i = start, j = 0; i < parent_internal_page->GetSize(); i++, j++) {
      new_internal_page->SetKeyValueAt(j, parent_internal_page->KeyAt(i), parent_internal_page->ValueAt(i));
      // modefy child page's parent page id
      Page *page = buffer_pool_manager_->FetchPage(parent_internal_page->ValueAt(i));
      auto tree_page = reinterpret_cast<BPlusTreePage *>(page->GetData());
      tree_page->SetParentPageId(new_page_id);
      buffer_pool_manager_->UnpinPage(tree_page->GetPageId(), true);
    }
    parent_internal_page->SetSize(internal_max_size_ - new_page_size + 1);
    new_internal_page->SetSize(new_page_size);

    // buffer_pool_manager_->UnpinPage(old_tree_page->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(new_tree_page->GetPageId(), true);
    old_tree_page = parent_internal_page;
    new_tree_page = new_internal_page;
    split_key = new_internal_page->KeyAt(0);
  }
  ReleaseWLatches(transaction);
  // buffer_pool_manager_->UnpinPage(old_tree_page->GetPageId(), true);
  buffer_pool_manager_->UnpinPage(new_tree_page->GetPageId(), true);
  return true;
}
/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immdiately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key, Transaction *transaction) {
  root_latch_.RLock();
  if (IsEmpty()) {
    root_latch_.RUnlock();
    return;
  }
  Page *page = GetLeafPage(key, transaction, Operation::Remove);
  auto leaf_page = reinterpret_cast<LeafPage *>(page->GetData());
  leaf_page->RemoveByKey(key, comparator_);
  if (leaf_page->GetSize() < leaf_page->GetMinSize()) {
    HandleOverFlow(leaf_page, transaction);
  }
  ReleaseWLatches(transaction);
  auto deleted_pages = transaction->GetDeletedPageSet();
  for (auto &pid : *deleted_pages) {
    buffer_pool_manager_->DeletePage(pid);
  }
  deleted_pages->clear();
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::GetSiblings(BPlusTreePage *tree_page, page_id_t &left_sibling_id, page_id_t &right_sibling_id,
                                 Transaction *transaction) {
  if (tree_page->IsRootPage()) {
    throw std::invalid_argument("No siblings here.");
  }
  // Page *page = buffer_pool_manager_->FetchPage(tree_page->GetParentPageId());
  Page *page = GetPageFromTransaction(tree_page->GetParentPageId(), transaction);
  auto parent_page = reinterpret_cast<InternalPage *>(page->GetData());
  int index = parent_page->FindIndexOfValue(tree_page->GetPageId());
  if (index == -1) {
    throw std::logic_error("child page id isn't in parent's value.");
  }
  left_sibling_id = right_sibling_id = INVALID_PAGE_ID;
  if (index != 0) {
    left_sibling_id = parent_page->ValueAt(index - 1);
  }
  if (index != parent_page->GetSize() - 1) {
    right_sibling_id = parent_page->ValueAt(index + 1);
  }
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::TryBorrow(BPlusTreePage *page, BPlusTreePage *sibling_page, InternalPage *parent_page, bool isLeft)
    -> bool {
  if (sibling_page == nullptr || sibling_page->GetSize() <= sibling_page->GetMinSize()) {
    return false;
  }
  // borrow last/first key-value from left/right sibling
  int sibling_borrow_at = isLeft ? sibling_page->GetSize() - 1 : (page->IsLeafPage() ? 0 : 1);
  int parent_update_at = parent_page->FindIndexOfValue(page->GetPageId()) + (isLeft ? 0 : 1);
  KeyType update_key;

  if (page->IsLeafPage()) {
    auto leaf_page = static_cast<LeafPage *>(page);
    auto leaf_sibling_page = static_cast<LeafPage *>(sibling_page);
    leaf_page->InsertByKey(leaf_sibling_page->KeyAt(sibling_borrow_at), leaf_sibling_page->ValueAt(sibling_borrow_at),
                           comparator_);
    leaf_sibling_page->RemoveByKey(leaf_sibling_page->KeyAt(sibling_borrow_at), comparator_);
    update_key = isLeft ? leaf_page->KeyAt(0) : leaf_sibling_page->KeyAt(0);

  } else {
    auto internal_page = static_cast<InternalPage *>(page);
    auto internal_sibling_page = static_cast<InternalPage *>(sibling_page);
    update_key = internal_sibling_page->KeyAt(sibling_borrow_at);
    page_id_t child_id;
    if (isLeft) {
      internal_page->InsertByKey(parent_page->KeyAt(parent_update_at), internal_page->ValueAt(0), comparator_);
      internal_page->SetKeyValueAt(0, internal_sibling_page->KeyAt(sibling_borrow_at),
                                   internal_sibling_page->ValueAt(sibling_borrow_at));
      child_id = internal_page->ValueAt(0);
    } else {
      internal_page->SetKeyValueAt(internal_page->GetSize(), parent_page->KeyAt(parent_update_at),
                                   internal_sibling_page->ValueAt(0));
      internal_page->IncreaseSize(1);
      internal_sibling_page->SetKeyValueAt(0, internal_sibling_page->KeyAt(0), internal_sibling_page->ValueAt(1));
      child_id = internal_page->ValueAt(internal_page->GetSize() - 1);
    }
    internal_sibling_page->RemoveByKey(internal_sibling_page->KeyAt(sibling_borrow_at), comparator_);
    Page *page = buffer_pool_manager_->FetchPage(child_id);
    auto child_page = reinterpret_cast<BPlusTreePage *>(page->GetData());
    child_page->SetParentPageId(internal_page->GetPageId());
    buffer_pool_manager_->UnpinPage(page->GetPageId(), true);
  }
  // update parent's key
  parent_page->SetKeyAt(parent_update_at, update_key);
  return true;
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::MergePage(BPlusTreePage *left_page, BPlusTreePage *right_page, InternalPage *parent_page) {
  if (left_page->IsLeafPage()) {
    auto left_leaf_page = static_cast<LeafPage *>(left_page);
    auto right_leaf_page = static_cast<LeafPage *>(right_page);
    // insert all key-value pairs in right page into left page.
    for (int i = 0; i < right_leaf_page->GetSize(); i++) {
      left_leaf_page->InsertByKey(right_leaf_page->KeyAt(i), right_leaf_page->ValueAt(i), comparator_);
    }
    left_leaf_page->SetNextPageId(right_leaf_page->GetNextPageId());
    // remove right page from parent.
    int remove_index = parent_page->FindIndexOfValue(right_page->GetPageId());
    parent_page->RemoveByKey(parent_page->KeyAt(remove_index), comparator_);
  } else {
    auto left_internal_page = static_cast<InternalPage *>(left_page);
    auto right_internal_page = static_cast<InternalPage *>(right_page);
    // move parent's key down into left page, the correspond value is right page's first value.
    left_internal_page->InsertByKey(parent_page->KeyAt(parent_page->FindIndexOfValue(right_page->GetPageId())),
                                    right_internal_page->ValueAt(0), comparator_);
    // set new parent
    Page *page = buffer_pool_manager_->FetchPage(right_internal_page->ValueAt(0));
    auto cur_page = reinterpret_cast<BPlusTreePage *>(page->GetData());
    cur_page->SetParentPageId(left_internal_page->GetPageId());
    buffer_pool_manager_->UnpinPage(cur_page->GetPageId(), true);

    parent_page->RemoveByKey(parent_page->KeyAt(parent_page->FindIndexOfValue(right_page->GetPageId())), comparator_);
    for (int i = 1; i < right_internal_page->GetSize(); i++) {
      left_internal_page->InsertByKey(right_internal_page->KeyAt(i), right_internal_page->ValueAt(i), comparator_);
      // update child's parent pointer
      Page *page = buffer_pool_manager_->FetchPage(right_internal_page->ValueAt(i));
      auto cur_page = reinterpret_cast<BPlusTreePage *>(page->GetData());
      cur_page->SetParentPageId(left_internal_page->GetPageId());
      buffer_pool_manager_->UnpinPage(cur_page->GetPageId(), true);
    }
  }
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ReleaseSiblings(page_id_t left_sibling_id, page_id_t right_sibling_id, Page *page_l,
                                     Page *page_r) {
  if (left_sibling_id != INVALID_PAGE_ID) {
    page_l->WUnlatch();
    buffer_pool_manager_->UnpinPage(left_sibling_id, true);
  }
  if (right_sibling_id != INVALID_PAGE_ID) {
    page_r->WUnlatch();
    buffer_pool_manager_->UnpinPage(right_sibling_id, true);
  }
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::HandleOverFlow(BPlusTreePage *page, Transaction *transaction) {
  if (page->IsRootPage()) {
    // If the node is leaf node, there is no limit in min size; Root node's min size is 1.
    if (page->GetSize() > 1 || (page->IsLeafPage() && page->GetSize() == 1)) {
      return;
    }
    if (page->IsLeafPage()) {
      transaction->AddIntoDeletedPageSet(page->GetPageId());
      root_page_id_ = INVALID_PAGE_ID;
    } else {
      // Root is an internal page and underflow, so the tree's height should decrease;
      BUSTUB_ASSERT(page->GetSize() > 0, "Internal page root size shouldn't be decreased to 0.");
      auto old_root_page = static_cast<InternalPage *>(page);
      root_page_id_ = old_root_page->ValueAt(0);
      Page *new_page = buffer_pool_manager_->FetchPage(root_page_id_);
      auto new_root_page = reinterpret_cast<BPlusTreePage *>(new_page->GetData());
      new_root_page->SetParentPageId(INVALID_PAGE_ID);
      transaction->AddIntoDeletedPageSet(page->GetPageId());
      buffer_pool_manager_->UnpinPage(root_page_id_, true);
    }
    UpdateRootPageId();
    return;
  }

  page_id_t left_sibling_id;
  page_id_t right_sibling_id;
  GetSiblings(page, left_sibling_id, right_sibling_id, transaction);
  if (left_sibling_id == INVALID_PAGE_ID && right_sibling_id == INVALID_PAGE_ID) {
    throw std::logic_error("This non-root page hasn't siblings");
    // Page *page_p = GetPageFromTransaction(page->GetParentPageId(), transaction);
    // auto parent_page = reinterpret_cast<InternalPage *>(page_p->GetData());
    // HandleOverFlow(parent_page, transaction);
    // return;
  }
  BPlusTreePage *left_sibling_page = nullptr;
  BPlusTreePage *right_sibling_page = nullptr;
  Page *page_l = nullptr;
  Page *page_r = nullptr;
  if (left_sibling_id != INVALID_PAGE_ID) {
    page_l = buffer_pool_manager_->FetchPage(left_sibling_id);
    page_l->WLatch();
    left_sibling_page = reinterpret_cast<BPlusTreePage *>(page_l->GetData());
  }
  if (right_sibling_id != INVALID_PAGE_ID) {
    page_r = buffer_pool_manager_->FetchPage(right_sibling_id);
    page_r->WLatch();
    right_sibling_page = reinterpret_cast<BPlusTreePage *>(page_r->GetData());
  }
  Page *page_p = GetPageFromTransaction(page->GetParentPageId(), transaction);
  auto parent_page = reinterpret_cast<InternalPage *>(page_p->GetData());

  if (TryBorrow(page, right_sibling_page, parent_page, false) ||
      TryBorrow(page, left_sibling_page, parent_page, true)) {
    ReleaseSiblings(left_sibling_id, right_sibling_id, page_l, page_r);
    return;
  }
  // both sibling has no enough Key-value, merge them into one node.
  BPlusTreePage *left_page;
  BPlusTreePage *right_page;
  if (right_sibling_page != nullptr) {
    left_page = page;
    right_page = right_sibling_page;
  } else {
    left_page = left_sibling_page;
    right_page = page;
  }
  MergePage(left_page, right_page, parent_page);
  transaction->AddIntoDeletedPageSet(right_page->GetPageId());
  // Unpinpage
  ReleaseSiblings(left_sibling_id, right_sibling_id, page_l, page_r);
  if (parent_page->GetSize() < parent_page->GetMinSize()) {
    HandleOverFlow(parent_page, transaction);
  }
  // buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the leftmost leaf node first, then construct
 * index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE {
  root_latch_.RLock();
  if (IsEmpty()) {
    root_latch_.RUnlock();
    return End();
  }
  page_id_t page_id = root_page_id_;
  Page *pre_page = nullptr;
  while (true) {
    Page *page = buffer_pool_manager_->FetchPage(page_id);
    page->RLatch();
    auto tree_page = reinterpret_cast<BPlusTreePage *>(page->GetData());
    if (pre_page != nullptr) {
      pre_page->RUnlatch();
      buffer_pool_manager_->UnpinPage(pre_page->GetPageId(), false);
    } else {
      root_latch_.RUnlock();
    }
    if (tree_page->IsLeafPage()) {
      return INDEXITERATOR_TYPE(page, 0, buffer_pool_manager_);
    }
    auto internal_page = static_cast<InternalPage *>(tree_page);
    if (internal_page == nullptr) {
      throw std::bad_cast();
    }
    page_id = internal_page->ValueAt(0);
    pre_page = page;
  }
}

/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE {
  root_latch_.RLock();
  Page *page = GetLeafPage(key, nullptr, Operation::Read);
  auto leaf_page = reinterpret_cast<LeafPage *>(page->GetData());
  int index = leaf_page->LowerBound(key, comparator_);
  return INDEXITERATOR_TYPE(page, index, buffer_pool_manager_);
}

/*
 * Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE { return INDEXITERATOR_TYPE(nullptr, 0, buffer_pool_manager_); }

/**
 * @return Page id of the root of this tree
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetRootPageId() -> page_id_t { return root_page_id_; }

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/
/*
 * Update/Insert root page id in header page(where page_id = 0, header_page is
 * defined under include/page/header_page.h)
 * Call this method everytime root page id is changed.
 * @parameter: insert_record      defualt value is false. When set to true,
 * insert a record <index_name, root_page_id> into header page instead of
 * updating it.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::UpdateRootPageId(int insert_record) {
  auto *header_page = static_cast<HeaderPage *>(buffer_pool_manager_->FetchPage(HEADER_PAGE_ID));
  if (insert_record != 0) {
    // create a new record<index_name + root_page_id> in header_page
    header_page->InsertRecord(index_name_, root_page_id_);
  } else {
    // update root_page_id in header_page
    header_page->UpdateRecord(index_name_, root_page_id_);
  }
  buffer_pool_manager_->UnpinPage(HEADER_PAGE_ID, true);
}

/*
 * This method is used for test only
 * Read data from file and insert one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertFromFile(const std::string &file_name, Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;

    KeyType index_key;
    index_key.SetFromInteger(key);
    RID rid(key);
    Insert(index_key, rid, transaction);
  }
}
/*
 * This method is used for test only
 * Read data from file and remove one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::RemoveFromFile(const std::string &file_name, Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;
    KeyType index_key;
    index_key.SetFromInteger(key);
    Remove(index_key, transaction);
  }
}

/**
 * This method is used for debug only, You don't need to modify
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Draw(BufferPoolManager *bpm, const std::string &outf) {
  if (IsEmpty()) {
    LOG_WARN("Draw an empty tree");
    return;
  }
  std::ofstream out(outf);
  out << "digraph G {" << std::endl;
  ToGraph(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(root_page_id_)->GetData()), bpm, out);
  out << "}" << std::endl;
  out.flush();
  out.close();
}

/**
 * This method is used for debug only, You don't need to modify
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Print(BufferPoolManager *bpm) {
  if (IsEmpty()) {
    LOG_WARN("Print an empty tree");
    return;
  }
  ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(root_page_id_)->GetData()), bpm);
}

/**
 * This method is used for debug only, You don't need to modify
 * @tparam KeyType
 * @tparam ValueType
 * @tparam KeyComparator
 * @param page
 * @param bpm
 * @param out
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToGraph(BPlusTreePage *page, BufferPoolManager *bpm, std::ofstream &out) const {
  std::string leaf_prefix("LEAF_");
  std::string internal_prefix("INT_");
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    // Print node name
    out << leaf_prefix << leaf->GetPageId();
    // Print node properties
    out << "[shape=plain color=green ";
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">P=" << leaf->GetPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">"
        << "max_size=" << leaf->GetMaxSize() << ",min_size=" << leaf->GetMinSize() << ",size=" << leaf->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < leaf->GetSize(); i++) {
      out << "<TD>" << leaf->KeyAt(i) << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Leaf node link if there is a next page
    if (leaf->GetNextPageId() != INVALID_PAGE_ID) {
      out << leaf_prefix << leaf->GetPageId() << " -> " << leaf_prefix << leaf->GetNextPageId() << ";\n";
      out << "{rank=same " << leaf_prefix << leaf->GetPageId() << " " << leaf_prefix << leaf->GetNextPageId() << "};\n";
    }

    // Print parent links if there is a parent
    if (leaf->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << leaf->GetParentPageId() << ":p" << leaf->GetPageId() << " -> " << leaf_prefix
          << leaf->GetPageId() << ";\n";
    }
  } else {
    auto *inner = reinterpret_cast<InternalPage *>(page);
    // Print node name
    out << internal_prefix << inner->GetPageId();
    // Print node properties
    out << "[shape=plain color=pink ";  // why not?
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">P=" << inner->GetPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">"
        << "max_size=" << inner->GetMaxSize() << ",min_size=" << inner->GetMinSize() << ",size=" << inner->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < inner->GetSize(); i++) {
      out << "<TD PORT=\"p" << inner->ValueAt(i) << "\">";
      if (i > 0) {
        out << inner->KeyAt(i);
      } else {
        out << " ";
      }
      out << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Parent link
    if (inner->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << inner->GetParentPageId() << ":p" << inner->GetPageId() << " -> " << internal_prefix
          << inner->GetPageId() << ";\n";
    }
    // Print leaves
    for (int i = 0; i < inner->GetSize(); i++) {
      auto child_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i))->GetData());
      ToGraph(child_page, bpm, out);
      if (i > 0) {
        auto sibling_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i - 1))->GetData());
        if (!sibling_page->IsLeafPage() && !child_page->IsLeafPage()) {
          out << "{rank=same " << internal_prefix << sibling_page->GetPageId() << " " << internal_prefix
              << child_page->GetPageId() << "};\n";
        }
        bpm->UnpinPage(sibling_page->GetPageId(), false);
      }
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

/**
 * This function is for debug only, you don't need to modify
 * @tparam KeyType
 * @tparam ValueType
 * @tparam KeyComparator
 * @param page
 * @param bpm
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToString(BPlusTreePage *page, BufferPoolManager *bpm) const {
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    std::cout << "Leaf Page: " << leaf->GetPageId() << " parent: " << leaf->GetParentPageId()
              << " next: " << leaf->GetNextPageId() << std::endl;
    for (int i = 0; i < leaf->GetSize(); i++) {
      std::cout << leaf->KeyAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
  } else {
    auto *internal = reinterpret_cast<InternalPage *>(page);
    std::cout << "Internal Page: " << internal->GetPageId() << " parent: " << internal->GetParentPageId() << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      std::cout << internal->KeyAt(i) << ": " << internal->ValueAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(internal->ValueAt(i))->GetData()), bpm);
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;
template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;
template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;
}  // namespace bustub
