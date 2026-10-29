#include "plugin/KeyboardEventQueue.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

namespace
{
void require(bool condition, const char* description)
{
    if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}
}

int main()
{
    using Queue = cs80::KeyboardEventQueue<4>;
    Queue queue;
    Queue::Batch batch;
    require(queue.push(1, 60, 0.8f, true, 10), "accept onset");
    require(queue.push(1, 60, 0, false, 12), "accept release");
    require(queue.take(batch) == 2 && batch[0].on && !batch[1].on, "FIFO onset/release");
    require(Queue::samplePosition(batch, 2, 0, 64) == 0, "first event at zero");
    require(Queue::samplePosition(batch, 2, 1, 64) == 43, "JUCE millisecond stretch");
    require(Queue::samplePosition(batch, 2, 1, 0) == 0, "zero-frame dispatch");
    require(Queue::samplePosition(batch, 2, 1, 1) == 0, "one-frame clamp");
    queue.requestReset();
    queue.acknowledge(queue.requestedEpoch());
    queue.push(1, 60, 1, true, 0xfffffffeu);
    queue.push(1, 61, 1, true, 1);
    require(queue.take(batch) == 2, "wrap batch");
    require(Queue::samplePosition(batch, 2, 1, 64) == 48, "millisecond wrap");
    queue.requestReset();
    queue.acknowledge(queue.requestedEpoch());
    queue.push(1, 60, 1, true, 10);
    queue.push(1, 61, 1, true, 9);
    queue.push(1, 62, 1, true, 11);
    require(queue.take(batch) == 3 && batch[1].milliseconds == 10,
            "regressed producer timestamp clamped in publication order");
    require(Queue::samplePosition(batch, 3, 1, 64) == 0
            && Queue::samplePosition(batch, 3, 2, 64) == 32, "normalized timestamps remain ordered");
    // Mapping is safe even for a synthetic malformed batch; real producers normalize it.
    batch[0].milliseconds = 100;
    batch[1].milliseconds = 99;
    batch[2].milliseconds = 100;
    require(Queue::samplePosition(batch, 3, 1, std::numeric_limits<int>::max())
                == std::numeric_limits<int>::max() - 1, "floating clamp precedes integer conversion");
    for (int i = 0; i < 4; ++i) require(queue.push(1, 60 + i, 1, true, 20), "fill capacity");
    require(!queue.push(1, 60, 0, false, 21) && queue.hasOverflow(), "lost off latches overflow");
    require(queue.take(batch) == 4, "consumer drains bounded batch");
    require(!queue.push(1, 61, 0, false, 22), "overflow stays latched after drain");
    queue.requestReset();
    const auto resetEpoch = queue.requestedEpoch();
    require(queue.needsAcknowledgment(), "reset requires UI ack");
    require(!queue.push(1, 60, 1, true, 23), "reject during reset");
    queue.acknowledge(resetEpoch - 1);
    require(queue.needsAcknowledgment(), "stale ack cannot reopen");
    queue.acknowledge(resetEpoch);
    require(!queue.hasOverflow() && !queue.needsAcknowledgment(), "UI ack reopens");
    queue.acknowledge(resetEpoch - 1);
    require(!queue.needsAcknowledgment(), "late stale acknowledgment cannot close current epoch");
    queue.push(1, 64, 1, true, 24);
    queue.requestReset();
    require(queue.take(batch) == 0, "reset drops queued old epoch");
    require(!queue.push(1, 66, 1, true, 25), "lifecycle-only reset rejects before acknowledgment");
    queue.acknowledge(queue.requestedEpoch());
    require(queue.push(1, 65, 1, true, 25) && queue.take(batch) == 1 && batch[0].note == 65,
            "new epoch delivers only new notes");
    require(!queue.push(0, 60, 1, true, 0) && !queue.push(1, 128, 1, true, 0), "invalid MIDI rejected");

    // Concurrent producers publish distinct channel sequences. Consumer never locks.
    using StressQueue = cs80::KeyboardEventQueue<32768>;
    // Keep stress storage off the stack (Windows commonly defaults to 1 MiB).
    // Both allocations occur before producers/consumer start; take() allocates nothing.
    auto concurrentStorage = std::make_unique<StressQueue>();
    auto batchStorage = std::make_unique<StressQueue::Batch>();
    auto& concurrent = *concurrentStorage;
    auto& concurrentBatch = *batchStorage;
    std::atomic<bool> start { false };
    std::vector<std::thread> producers;
    for (int channel = 1; channel <= 4; ++channel)
        producers.emplace_back([&, channel]
        {
            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int i = 0; i < 4000; ++i)
                require(concurrent.push(channel, i % 128, (float)i, (i % 2) == 0, (unsigned)i), "serialized producer accepted");
        });
    start.store(true, std::memory_order_release);
    std::array<int, 4> next {};
    int received = 0;
    while (received < 16000)
    {
        const auto count = concurrent.take(concurrentBatch);
        for (size_t i = 0; i < count; ++i)
        {
            const auto& e = concurrentBatch[i];
            auto& expected = next[(size_t)e.channel - 1];
            require(e.velocity == (float)expected && e.note == expected % 128,
                    "each serialized producer preserves order");
            ++expected;
            ++received;
        }
        if (count == 0) std::this_thread::yield();
    }
    for (auto& producer : producers) producer.join();
    require(!concurrent.hasOverflow(), "concurrent consumer has no overflow");
    std::cout << "Keyboard event queue tests passed\n";
}
