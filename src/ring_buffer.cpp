#include "classcaption/ring_buffer.h"

#include <algorithm>

using namespace std;

namespace classcaption {

RingBuffer::RingBuffer(size_t capacity) : storage_(capacity) {}

size_t RingBuffer::push(span<const float> samples) {
    // Only this thread moves written_, so a relaxed load sees its own latest value.
    const size_t written = written_.load(memory_order_relaxed);
    const size_t free_slots = storage_.size() - (written - read_.load(memory_order_acquire));
    const size_t count = min(samples.size(), free_slots);

    const size_t start = written % storage_.size();
    const size_t before_wrap = min(count, storage_.size() - start);
    ranges::copy(samples.first(before_wrap), storage_.begin() + static_cast<ptrdiff_t>(start));
    ranges::copy(samples.subspan(before_wrap, count - before_wrap), storage_.begin());

    // Release publishes the copied samples before the consumer can see the new count.
    written_.store(written + count, memory_order_release);
    return count;
}

size_t RingBuffer::pop(span<float> out) {
    const size_t read = read_.load(memory_order_relaxed);
    const size_t available = written_.load(memory_order_acquire) - read;
    const size_t count = min(out.size(), available);

    const size_t start = read % storage_.size();
    const size_t before_wrap = min(count, storage_.size() - start);
    const auto first = storage_.begin() + static_cast<ptrdiff_t>(start);
    copy(first, first + static_cast<ptrdiff_t>(before_wrap), out.begin());
    copy(storage_.begin(), storage_.begin() + static_cast<ptrdiff_t>(count - before_wrap),
         out.begin() + static_cast<ptrdiff_t>(before_wrap));

    read_.store(read + count, memory_order_release);
    return count;
}

size_t RingBuffer::size() const {
    return written_.load(memory_order_acquire) - read_.load(memory_order_acquire);
}

} // namespace classcaption
