module;

#include <functional>
#include <unordered_set>

export module DataStructures;

class Constants16bit
{
public:
    static constexpr uint16_t Invalid  = 0x8000U; // 10000000 00000000
};

template<typename T>
requires std::is_default_constructible_v<T>
class LinkedListNode // not exported
{
    T element {};
    LinkedListNode<T>& operator=(const T& other) { element = other; return *this; }
};

export template<typename T, uint16_t Size>
class LinkedListArray
{
    static_assert(Size < Constants16bit::Invalid);
public:
    LinkedListArray();
    bool Insert(const T& elem);

    void ForEach(std::function<bool(T)> iterate); // iterate returns 'true' is element should be removed
    void PostIterate(); // cleanup any elements marked as so

private:
    LinkedListNode<T> mList[Size];
    std::unordered_set<uint16_t> mAllocated;

    // free-list implemented as a stack (probably better cache performance)
    uint16_t mFreeList[Size];
    uint16_t mFreeCount;

    // elements marked for removal
    uint16_t mRemovals[Size];
    uint16_t mRemovalCount;
};

module :private;

template <typename T, uint16_t Size>
LinkedListArray<T, Size>::LinkedListArray()
{
    for (uint16_t i = 0; i < Size; i++)
    {
        mFreeList[i] = i;
        mRemovals[i] = Constants16bit::Invalid;
    }
    mFreeCount = Size;
    mRemovalCount = 0U;
}

template <typename T, uint16_t Size>
bool LinkedListArray<T, Size>::Insert(const T& elem)
{
    if (mFreeCount == 0) { return false; }
    const uint16_t index = mFreeList[--mFreeCount];
    mList[index] = elem; // insert at back
    mAllocated.insert(index);
    return true;
}

template <typename T, uint16_t Size>
void LinkedListArray<T, Size>::ForEach(std::function<bool(T)> iterate)
{
    for (const uint16_t index : mAllocated)
    {
        const uint16_t elem = mList[index]; 
        if (iterate(elem))
        {
            mRemovals[mRemovalCount++] = index;
        }
    }
}

template <typename T, uint16_t Size>
void LinkedListArray<T, Size>::PostIterate()
{
    for (uint16_t i = 0; i < mRemovalCount; i++)
    {
        mAllocated.erase(mRemovals[i]);
        mFreeList[mFreeCount++] = mRemovals[i];
    }
}
