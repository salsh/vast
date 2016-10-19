#include "vast/null_bitmap.hpp"

namespace vast {

null_bitmap::null_bitmap(size_type n, bool bit) {
  append_bits(bit, n);
}

bool null_bitmap::empty() const {
  return bitvector_.empty();
}

null_bitmap::size_type null_bitmap::size() const {
  return bitvector_.size();
}

void null_bitmap::append_bit(bool bit) {
  bitvector_.push_back(bit);
}

void null_bitmap::append_bits(bool bit, size_type n) {
  bitvector_.resize(bitvector_.size() + n, bit);
}

void null_bitmap::append_block(block_type value, size_type bits) {
  bitvector_.append_block(value, bits);
}

void null_bitmap::flip() {
  bitvector_.flip();
}

bool operator==(null_bitmap const& x, null_bitmap const& y) {
  return x.bitvector_ == y.bitvector_;
}


null_bitmap_range::null_bitmap_range(null_bitmap const& bm)
  : bitvector_{bm.empty() ? nullptr : &bm.bitvector_},
    block_{bm.bitvector_.blocks().begin()} {
  if (bitvector_)
    scan();
}

void null_bitmap_range::next() {
  if (block_ == bitvector_->blocks().end())
    bitvector_ = nullptr;
  else
    scan();
}

bool null_bitmap_range::done() const {
  return bitvector_ == nullptr;
}

void null_bitmap_range::scan() {
  VAST_ASSERT(bitvector_ != nullptr);
  VAST_ASSERT(block_ != bitvector_->blocks().end());
  using word_type = null_bitmap::bitvector_type::word;
  auto end = bitvector_->blocks().end();
  auto last = end - 1;
  if (block_ == last) {
    // Process the last block.
    auto partial = bitvector_->size() % word_type::width;
    bits_ = {*block_, partial == 0 ? word_type::width : partial};
    ++block_;
  } else if (!word_type::all_or_none(*block_)) {
    // Process an intermediate inhomogeneous block.
    bits_ = {*block_, word_type::width};
    ++block_;
  } else {
    // Scan for consecutive runs of all-0 or all-1 blocks.
    auto n = word_type::width;
    auto data = *block_;
    while (++block_ != last && *block_ == data)
      n += word_type::width;
    if (block_ == last) {
      auto partial = bitvector_->size() % word_type::width;
      if (partial > 0) {
        auto mask = word_type::mask(partial);
        if ((*block_ & mask) == (data & mask)) {
          n += partial;
          ++block_;
        }
      } else if (*block_ == data) {
        n += word_type::width;
        ++block_;
      }
    }
    bits_ = {data, n};
  }
}

null_bitmap_range bit_range(null_bitmap const& bm) {
  return null_bitmap_range{bm};
}

} // namespace vast
