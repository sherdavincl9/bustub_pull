//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// extendible_hash_table.cpp
//
// Identification: src/container/hash/extendible_hash_table.cpp
//
// Copyright (c) 2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <cassert>
#include <cstdlib>
#include <functional>
#include <list>
#include <utility>

#include "container/hash/extendible_hash_table.h"
#include "storage/page/page.h"

namespace bustub {

template <typename K, typename V>
ExtendibleHashTable<K, V>::ExtendibleHashTable(size_t bucket_size)
    : global_depth_(0), bucket_size_(bucket_size), num_buckets_(1), dir_(1, std::make_shared<Bucket>(bucket_size)) {}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::IndexOf(const K &key) -> size_t {
  int mask = (1 << global_depth_) - 1;
  return std::hash<K>()(key) & mask;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetGlobalDepth() const -> int {
  std::scoped_lock<std::mutex> lock(latch_);
  return GetGlobalDepthInternal();
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetGlobalDepthInternal() const -> int {
  return global_depth_;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetLocalDepth(int dir_index) const -> int {
  std::scoped_lock<std::mutex> lock(latch_);
  return GetLocalDepthInternal(dir_index);
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetLocalDepthInternal(int dir_index) const -> int {
  return dir_[dir_index]->GetDepth();
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetNumBuckets() const -> int {
  std::scoped_lock<std::mutex> lock(latch_);
  return GetNumBucketsInternal();
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetNumBucketsInternal() const -> int {
  return num_buckets_;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Find(const K &key, V &value) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  size_t index = IndexOf(key);
  std::shared_ptr<Bucket> bucket = dir_[index];
  return bucket->Find(key, value);
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Remove(const K &key) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  size_t index = IndexOf(key);
  std::shared_ptr<Bucket> bucket = dir_[index];
  return bucket->Remove(key);
}

template <typename K, typename V>
void ExtendibleHashTable<K, V>::Insert(const K &key, const V &value) {
  std::scoped_lock<std::mutex> lock(latch_);
  size_t index = IndexOf(key);
  std::shared_ptr<Bucket> curbucket = dir_[index];
  if (curbucket->Insert(key, value)) {
    return;
  }
  if (curbucket->GetDepth() == GetGlobalDepthInternal()) {
    size_t length = dir_.size();
    dir_.reserve(2 * length);
    std::copy_n(dir_.begin(), length, std::back_inserter(dir_));
    global_depth_++;
  }
  curbucket = dir_[index];
  if (curbucket->GetDepth() < GetGlobalDepthInternal()) {
    auto bucket0 = std::make_shared<Bucket>(bucket_size_, curbucket->GetDepth() + 1);
    auto bucket1 = std::make_shared<Bucket>(bucket_size_, curbucket->GetDepth() + 1);
    num_buckets_++;
    int mask = 1 << curbucket->GetDepth();
    for (auto &[k, v] : curbucket->GetItems()) {
      size_t hashkey = std::hash<K>()(k);
      if ((hashkey & mask) != 0) {
        bucket1->Insert(k, v);
      } else {
        bucket0->Insert(k, v);
      }
    }
    for (size_t i = index & (mask - 1); i < dir_.size(); i += mask) {
      if ((i & mask) != 0) {
        dir_[i] = bucket1;
      } else {
        dir_[i] = bucket0;
      }
    }
    latch_.unlock();
    Insert(key, value);
    latch_.lock();
  }
}

//===--------------------------------------------------------------------===//
// Bucket
//===--------------------------------------------------------------------===//
template <typename K, typename V>
ExtendibleHashTable<K, V>::Bucket::Bucket(size_t array_size, int depth) : size_(array_size), depth_(depth) {}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Find(const K &key, V &value) -> bool {
  for (const auto &[k, v] : list_) {
    if (key == k) {
      value = v;
      return true;
    }
  }
  return false;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Remove(const K &key) -> bool {
  for (auto iter = list_.begin(); iter != list_.end(); ++iter) {
    if ((*iter).first == key) {
      list_.erase(iter);
      return true;
    }
  }
  return false;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Insert(const K &key, const V &value) -> bool {
  for (auto &[k, v] : list_) {
    if (key == k) {
      v = value;
      return true;
    }
  }
  if (!IsFull()) {
    list_.emplace_back(key, value);
    return true;
  }
  return false;
}

template class ExtendibleHashTable<page_id_t, Page *>;
template class ExtendibleHashTable<Page *, std::list<Page *>::iterator>;
template class ExtendibleHashTable<int, int>;
// test purpose
template class ExtendibleHashTable<int, std::string>;
template class ExtendibleHashTable<int, std::list<int>::iterator>;

}  // namespace bustub
