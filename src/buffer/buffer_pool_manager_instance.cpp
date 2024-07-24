//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// buffer_pool_manager_instance.cpp
//
// Identification: src/buffer/buffer_pool_manager.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/buffer_pool_manager_instance.h"

#include "common/exception.h"
#include "common/macros.h"

namespace bustub {

BufferPoolManagerInstance::BufferPoolManagerInstance(size_t pool_size, DiskManager *disk_manager, size_t replacer_k,
                                                     LogManager *log_manager)
    : pool_size_(pool_size), disk_manager_(disk_manager), log_manager_(log_manager) {
  // we allocate a consecutive memory space for the buffer pool
  pages_ = new Page[pool_size_];
  page_table_ = new ExtendibleHashTable<page_id_t, frame_id_t>(bucket_size_);
  replacer_ = new LRUKReplacer(pool_size, replacer_k);

  // Initially, every page is in the free list.
  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.emplace_back(static_cast<int>(i));
  }

  // TODO(students): remove this line after you have implemented the buffer pool manager
  // throw NotImplementedException(
  //     "BufferPoolManager is not implemented yet. If you have finished implementing BPM, please remove the throw "
  //     "exception line in `buffer_pool_manager_instance.cpp`.");
}

BufferPoolManagerInstance::~BufferPoolManagerInstance() {
  delete[] pages_;
  delete page_table_;
  delete replacer_;
}

auto BufferPoolManagerInstance::NewPgImp(page_id_t *page_id) -> Page * {
  std::scoped_lock<std::mutex> lock(latch_);
  frame_id_t frame_id;
  if (GetFrame(&frame_id)) {
    page_id_t new_page_id = AllocatePage();
    Page *new_page = &pages_[frame_id];
    new_page->page_id_ = new_page_id;
    new_page->pin_count_ = 1;
    new_page->ResetMemory();
    replacer_->RecordAccess(frame_id);
    replacer_->SetEvictable(frame_id, false);
    page_table_->Insert(new_page_id, frame_id);
    // new_page->is_dirty_ = false;
    *page_id = new_page_id;
    return new_page;
  }
  return nullptr;
}

auto BufferPoolManagerInstance::FetchPgImp(page_id_t page_id) -> Page * {
  std::scoped_lock<std::mutex> lock(latch_);
  frame_id_t frame_id;
  if (page_table_->Find(page_id, frame_id)) {
    Page *page = &pages_[frame_id];
    replacer_->SetEvictable(frame_id, false);
    page->pin_count_++;
    return page;
  }
  if (GetFrame(&frame_id)) {
    Page *new_page = &pages_[frame_id];
    disk_manager_->ReadPage(page_id, new_page->GetData());
    new_page->page_id_ = page_id;
    new_page->pin_count_ = 1;
    replacer_->RecordAccess(frame_id);
    replacer_->SetEvictable(frame_id, false);
    // page_table_[page_id] = frame_id;
    page_table_->Insert(page_id, frame_id);
    new_page->is_dirty_ = false;
    return new_page;
  }
  return nullptr;
}

auto BufferPoolManagerInstance::UnpinPgImp(page_id_t page_id, bool is_dirty) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  frame_id_t frame_id;
  if (!page_table_->Find(page_id, frame_id)) {
    return false;
  }
  Page *unpinned_page = &pages_[frame_id];
  if (is_dirty) {
    unpinned_page->is_dirty_ = true;
  }
  if (unpinned_page->pin_count_ <= 0) {
    return false;
  }
  unpinned_page->pin_count_--;
  if (unpinned_page->pin_count_ == 0) {
    replacer_->SetEvictable(frame_id, true);
  }
  return true;
}

auto BufferPoolManagerInstance::FlushPgImp(page_id_t page_id) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  frame_id_t frame_id;
  if (!page_table_->Find(page_id, frame_id) || page_id == INVALID_PAGE_ID) {
    return false;
  }
  disk_manager_->WritePage(page_id, pages_[frame_id].GetData());
  pages_[frame_id].is_dirty_ = false;
  return true;
}

void BufferPoolManagerInstance::FlushAllPgsImp() {
  // std::scoped_lock<std::mutex> lock(latch_);
  for (size_t i = 0; i < pool_size_; i++) {
    if (pages_[i].page_id_ != INVALID_PAGE_ID) {
      FlushPgImp(pages_[i].page_id_);
    }
  }
}

auto BufferPoolManagerInstance::DeletePgImp(page_id_t page_id) -> bool {
  latch_.lock();
  frame_id_t frame_id;
  if (!page_table_->Find(page_id, frame_id)) {
    latch_.unlock();
    return true;
  }
  if (pages_[frame_id].pin_count_ > 0) {
    latch_.unlock();
    return false;
  }
  Page *page = &pages_[frame_id];
  if (page->is_dirty_) {
    latch_.unlock();
    FlushPgImp(page_id);
    latch_.lock();
  }
  // reset metadata
  page->ResetMemory();
  page->is_dirty_ = false;
  page->pin_count_ = 0;
  page->page_id_ = INVALID_PAGE_ID;
  replacer_->Remove(frame_id);
  free_list_.push_back(frame_id);
  page_table_->Remove(page_id);
  DeallocatePage(page_id);
  latch_.unlock();
  return true;
}

auto BufferPoolManagerInstance::AllocatePage() -> page_id_t { return next_page_id_++; }

}  // namespace bustub
