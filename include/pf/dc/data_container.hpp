#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <type_traits>
#include <vector>

namespace pf::dc
{

// Forward declarations
template <typename T>
class Mempool;
template <typename T>
class SenderPort;
template <typename T>
class ReceiverPort;

// ---------------------------------------------------------------------------
// MempoolBuffer<T> — one typed slot in a Mempool<T>.
// Ref-counted: when refs reach 0 the slot is returned to its owning pool.
// ---------------------------------------------------------------------------
template <typename T>
struct MempoolBuffer
{
    T                data{};
    std::atomic<int> refs{0};
    int              index = -1;
    Mempool<T>*      pool  = nullptr;
};

// ---------------------------------------------------------------------------
// Mempool<T> — fixed pool of pre-constructed T slots with ring-buffer eviction.
//
// acquire() walks the ring starting from the last eviction point and returns
// the first slot whose ref count is 0 (i.e. not held by any receiver).
// When every slot is still referenced by a live receiver, acquire() returns
// nullptr — the caller should drop the send rather than corrupt live data.
//
// Thread-safe: acquire() / release() use an internal mutex.
// Non-copyable/movable: MempoolBuffer<T> slots hold a raw pointer back here.
// ---------------------------------------------------------------------------
template <typename T>
class Mempool
{
public:
    explicit Mempool(size_t poolCount)
        : m_slots(poolCount)
        , m_ringHead(0)
    {
        for (int i = 0; i < static_cast<int>(poolCount); ++i)
        {
            m_slots[i].index = i;
            m_slots[i].pool  = this;
        }
    }

    Mempool(const Mempool&)            = delete;
    Mempool& operator=(const Mempool&) = delete;
    Mempool(Mempool&&)                 = delete;
    Mempool& operator=(Mempool&&)      = delete;
    ~Mempool()                         = default;

    // Acquire a slot using ring-buffer eviction.
    // Scans from m_ringHead and returns the first unreferenced slot (refs == 0).
    // That slot's refs is set to 1.  Returns nullptr only when every slot is
    // still held by a live receiver (all refs > 0).
    MempoolBuffer<T>* acquire()
    {
        std::lock_guard<std::mutex> lock(m_mu);
        const int n = static_cast<int>(m_slots.size());
        for (int i = 0; i < n; ++i)
        {
            int idx = (m_ringHead + i) % n;
            if (m_slots[idx].refs.load() == 0)
            {
                m_ringHead = (idx + 1) % n;   // advance head past this slot
                m_slots[idx].data = T{};       // reset to default value
                m_slots[idx].refs.store(1);
                return &m_slots[idx];
            }
        }
        return nullptr; // all slots are live — cannot evict safely
    }

    // Decrement the ref count.  The slot becomes evictable when refs reach 0.
    void release(MempoolBuffer<T>& buf)
    {
        buf.refs.fetch_sub(1); // no free-list needed; acquire() scans the ring
    }

    size_t poolCount() const
    {
        return m_slots.size();
    }

private:
    std::vector<MempoolBuffer<T>> m_slots;
    int                           m_ringHead; // next candidate index for eviction
    std::mutex                    m_mu;
};

// ---------------------------------------------------------------------------
// SenderPort<T> — reserves a T slot from a Mempool<T> and delivers it to
// all connected ReceiverPort<T> instances (fan-out, zero-copy).
//
// Typical call sequence (any thread):
//   T* slot = sender.reserve();
//   // fill *slot ...
//   sender.deliver();   // fans out to every connected receiver
//
// Sender and its Mempool are intended to be static/public so any part of
// the application (e.g. a command registry) can push data at any time.
// ---------------------------------------------------------------------------
template <typename T>
class SenderPort
{
public:
    SenderPort() = default;
    ~SenderPort()
    {
        MempoolBuffer<T>*             toRelease;
        std::vector<ReceiverPort<T>*> toNotify;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            toRelease  = m_reserved;
            m_reserved = nullptr;
            m_mempool  = nullptr;
            toNotify   = std::move(m_receivers);
        }
        if (toRelease)
            toRelease->pool->release(*toRelease);
        for (auto* r : toNotify)
            r->onSenderDestroyed(*this);
    }

    SenderPort(const SenderPort&)            = delete;
    SenderPort& operator=(const SenderPort&) = delete;

    // ----- mempool connection ------------------------------------------------
    void connectMempool(Mempool<T>& mp)
    {
        MempoolBuffer<T>* toRelease = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            toRelease  = m_reserved;
            m_reserved = nullptr;
            m_mempool  = &mp;
        }
        if (toRelease)
            toRelease->pool->release(*toRelease);
    }

    void disconnectMempool()
    {
        MempoolBuffer<T>* toRelease = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            toRelease  = m_reserved;
            m_reserved = nullptr;
            m_mempool  = nullptr;
        }
        if (toRelease)
            toRelease->pool->release(*toRelease);
    }

    // ----- receiver connection -----------------------------------------------
    // Prefer ReceiverPort<T>::connect() which calls this automatically.
    void connect(ReceiverPort<T>& recv)
    {
        std::lock_guard<std::mutex> lock(m_mu);
        if (std::find(m_receivers.begin(), m_receivers.end(), &recv) == m_receivers.end())
            m_receivers.push_back(&recv);
    }

    void disconnect(ReceiverPort<T>& recv)
    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_receivers.erase(std::remove(m_receivers.begin(), m_receivers.end(), &recv), m_receivers.end());
    }

    // ----- data API ----------------------------------------------------------
    // Reserve a slot from the mempool for zero-copy writing.
    // Returns a pointer to T, or nullptr if no mempool is connected or the
    // pool is exhausted.  Any previously unreleased reservation is discarded.
    T* reserve()
    {
        MempoolBuffer<T>* toRelease = nullptr;
        MempoolBuffer<T>* acquired  = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            if (!m_mempool)
                return nullptr;
            toRelease  = m_reserved;
            m_reserved = nullptr;
            acquired   = m_mempool->acquire();
            m_reserved = acquired;
        }
        if (toRelease)
            toRelease->pool->release(*toRelease);
        return acquired ? &acquired->data : nullptr;
    }

    // Deliver the reserved slot to all connected receivers (fan-out), then
    // clear the reservation.  The slot's lifetime is managed by ref-counting.
    void deliver()
    {
        MempoolBuffer<T>*             buf = nullptr;
        std::vector<ReceiverPort<T>*> targets;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            if (!m_reserved)
                return;
            buf        = m_reserved;
            m_reserved = nullptr;
            targets    = m_receivers; // snapshot under lock
        }
        // Bump ref count for each receiver before releasing the sender's ref,
        // so the count never prematurely hits zero.
        buf->refs.fetch_add(static_cast<int>(targets.size()));
        for (auto* r : targets)
            r->push(*buf);
        buf->pool->release(*buf); // release sender's original ref
    }

    bool isConnected() const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return !m_receivers.empty();
    }

private:
    Mempool<T>*                   m_mempool  = nullptr;
    MempoolBuffer<T>*             m_reserved = nullptr;
    std::vector<ReceiverPort<T>*> m_receivers;
    mutable std::mutex            m_mu;
};

// ---------------------------------------------------------------------------
// ReceiverPort<T> — receives T slots delivered by a SenderPort<T>.
// Uses a double-buffer (pending → active) designed for main-loop polling.
//
// Typical usage (main loop):
//   // --- top of loop ---
//   receiver.update();            // promote latest pending slot to active
//   if (receiver.hasNewData()) {
//       const T* val = receiver.getData();   // zero-copy read
//       // use *val ...
//   }
//   // --- rest of loop ---
//   // getData() remains valid here
//   // --- bottom of loop ---
//   receiver.cleanup();           // clear the hasNewData flag
// ---------------------------------------------------------------------------
template <typename T>
class ReceiverPort
{
public:
    ReceiverPort() = default;
    ~ReceiverPort()
    {
        disconnect();
    }

    ReceiverPort(const ReceiverPort&)            = delete;
    ReceiverPort& operator=(const ReceiverPort&) = delete;

    // ----- sender connection -------------------------------------------------
    void connect(SenderPort<T>& sp)
    {
        disconnect(); // drop any existing connection first
        {
            std::lock_guard<std::mutex> lock(m_mu);
            m_sender = &sp;
        }
        sp.connect(*this);
    }

    void disconnect()
    {
        SenderPort<T>*    sp = nullptr;
        MempoolBuffer<T>* ab = nullptr;
        MempoolBuffer<T>* pb = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            sp             = m_sender;
            m_sender       = nullptr;
            ab             = m_activeBuf;
            m_activeBuf    = nullptr;
            pb             = m_pendingBuf;
            m_pendingBuf   = nullptr;
            m_newDataFlag  = false;
        }
        if (ab)
            ab->pool->release(*ab);
        if (pb)
            pb->pool->release(*pb);
        // Call sp->disconnect outside our lock to avoid lock-ordering deadlock
        // (deliver() holds sender.m_mu then calls push() → recv.m_mu).
        if (sp)
            sp->disconnect(*this);
    }

    // ----- main-loop API -----------------------------------------------------
    // Promote the pending slot (if any) to active; sets hasNewData.
    void update()
    {
        MempoolBuffer<T>* toRelease = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            if (m_pendingBuf)
            {
                toRelease    = m_activeBuf;
                m_activeBuf  = m_pendingBuf;
                m_pendingBuf = nullptr;
                m_newDataFlag = true;
            }
            else
            {
                m_newDataFlag = false;
            }
        }
        if (toRelease)
            toRelease->pool->release(*toRelease);
    }

    // Clear the hasNewData flag (call at end of main-loop iteration).
    void cleanup()
    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_newDataFlag = false;
    }

    // true if an active slot is present (persists across frames until replaced)
    bool hasData() const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_activeBuf != nullptr;
    }

    // true only between update() and cleanup()
    bool hasNewData() const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_newDataFlag;
    }

    // Returns a pointer to the active T for zero-copy reading, or nullptr.
    // Valid until the next update() call.
    const T* getData() const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_activeBuf ? &m_activeBuf->data : nullptr;
    }

    bool isConnected() const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_sender != nullptr;
    }

    // ----- internal (called by SenderPort) -----------------------------------
    void push(MempoolBuffer<T>& buf)
    {
        MempoolBuffer<T>* toRelease = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            if (!m_sender)
            {
                // Already disconnected — reject the incoming ref.
                toRelease = &buf;
            }
            else
            {
                // Latest-wins: discard any pending slot not yet promoted.
                toRelease    = m_pendingBuf;
                m_pendingBuf = &buf;
            }
        }
        if (toRelease)
            toRelease->pool->release(*toRelease);
    }

    void onSenderDestroyed(SenderPort<T>& sp)
    {
        std::lock_guard<std::mutex> lock(m_mu);
        if (m_sender == &sp)
            m_sender = nullptr;
    }

private:
    SenderPort<T>*     m_sender      = nullptr;
    MempoolBuffer<T>*  m_activeBuf   = nullptr;
    MempoolBuffer<T>*  m_pendingBuf  = nullptr;
    bool               m_newDataFlag = false;
    mutable std::mutex m_mu;
};

} // namespace pf::dc
