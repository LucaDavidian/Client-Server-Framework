#ifndef THREADSAFE_QUEUE_H
#define THREADSAFE_QUEUE_H

#include <mutex>
#include <cstddef>
#include "Vector.h"

using std::size_t;

template <typename T, template <typename> class S = Vector>
class ThreadsafeQueue
{
public:
    ThreadsafeQueue() = default;
    ~ThreadsafeQueue() { Clear(); }

    bool Empty() const { std::lock_guard<std::mutex> guard(mMutex); return mContainer.Empty(); }
    size_t Size() const { std::lock_guard<std::mutex> guard(mMutex); return mContainer.Size(); }

    void Clear() { std::lock_guard<std::mutex> guard(mMutex); mContainer.Clear(); }

    template <typename U>
    void EnQueue(U &&element) { std::lock_guard<std::mutex> guard(mMutex); mContainer.InsertLast(std::forward<U>(element)); }
    void DeQueue() { std::lock_guard<std::mutex> guard(mMutex); mContainer.RemoveFirst(); }

    T Front() const { std::lock_guard<std::mutex> guard(mMutex); return mContainer.First(); }
    T Back() const { std::lock_guard<std::mutex> guard(mMutex); return mContainer.Last(); }
private:
    mutable std::mutex mMutex;
    S<T> mContainer;
};

#endif  // THREADSAFE_QUEUE_H