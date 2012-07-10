#    Implementation of Arena.h functions

#ifndef FOLLY_ARENA_H_
#error This file may only be included from Arena.h
#endif

// Implementation of Arena.h functions

namespace folly {

template <class Alloc>
std::pair<typename Arena<Alloc>::Block*, size_t>
Arena<Alloc>::Block::allocate(Alloc& alloc, size_t size, bool allowSlack) {
  size_t allocSize = sizeof(Block) + size;
  if (allowSlack) {
    allocSize = ArenaAllocatorTraits<Alloc>::goodSize(alloc, allocSize);
  }

  void* mem = alloc.allocate(allocSize);
  assert(isAligned(mem));
  return std::make_pair(new (mem) Block(), allocSize - sizeof(Block));
}

template <class Alloc>
void Arena<Alloc>::Block::deallocate(Alloc& alloc) {
  this->~Block();
  alloc.deallocate(this);
}

template <class Alloc>
void* Arena<Alloc>::allocateSlow(size_t size) {
  std::pair<Block*, size_t> p;
  char* start;
  if (size > minBlockSize()) {
    // Allocate a large block for this chunk only, put it at the back of the
    // list so it doesn't get used for small allocations; don't change ptr_
    // and end_, let them point into a normal block (or none, if they're
    // null)
    p = Block::allocate(alloc(), size, false);
    start = p.first->start();
    blocks_.push_back(*p.first);
  } else {
    // Allocate a normal sized block and carve out size bytes from it
    p = Block::allocate(alloc(), minBlockSize(), true);
    start = p.first->start();
    blocks_.push_front(*p.first);
    ptr_ = start + size;
    end_ = start + p.second;
  }

  assert(p.second >= size);
  return start;
}

template <class Alloc>
void Arena<Alloc>::merge(Arena<Alloc>&& other) {
  blocks_.splice_after(blocks_.before_begin(), other.blocks_);
  other.blocks_.clear();
  other.ptr_ = other.end_ = nullptr;
}

template <class Alloc>
Arena<Alloc>::~Arena() {
  auto disposer = [this] (Block* b) { b->deallocate(this->alloc()); };
  while (!blocks_.empty()) {
    blocks_.pop_front_and_dispose(disposer);
  }
}

}  // namespace folly
