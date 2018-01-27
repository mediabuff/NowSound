// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

namespace NowSound
{
    template<typename T>
    struct Buf
    {
        const int Id;
        const T[] Data;

        public Buf(int id, T[] data)
        {
            Id = id;
            Data = data;
        }

        public bool Equals(Buf<T> other)
        {
            return Id == other.Id && Data == other.Data;
        }
    }

    // 
    // Allocate T[] of a predetermined size, and support returning such T[] to a free list.
    // 
    template<typename T>
    class BufferAllocator<T>
    {
    private:
        int _latestBufferId = 1; // 0 = empty buf

        // The number of T in a buffer from this allocator.
    public:
        const int BufferSize;

    private:
        // Free list; we recycle from here if possible.
        const std::vector<Buf<T>> _freeList;

        readonly int _sizeOfT;

        // 
        // Total number of buffers we have ever allocated.
        // 
        int _totalBufferCount;

        public BufferAllocator(int bufferSize, int initialBufferCount, int sizeOfT)
        {
            BufferSize = bufferSize;
            _sizeOfT = sizeOfT;

            for (int i = 0; i < initialBufferCount; i++) {
                _freeList.Add(new Buf<T>(_latestBufferId++, new T[BufferSize]));
            }
            _totalBufferCount = initialBufferCount;
        }

        // 
        // Number of bytes reserved by this allocator; will increase if free list runs out, and includes free space.
        // 
        public long TotalReservedSpace{ get{ return _totalBufferCount * BufferSize; } }

            // 
            // Number of bytes held in buffers on the free list.
            // 
        public long TotalFreeListSpace{ get{ return _freeList.Count * BufferSize; } }

            public Buf<T> Allocate()
        {
            if (_freeList.Count == 0) {
                _totalBufferCount++;
                return new Buf<T>(_latestBufferId++, new T[BufferSize]);
            }
            else {
                Buf<T> ret = _freeList[_freeList.Count - 1];
                _freeList.RemoveAt(_freeList.Count - 1);
                return ret;
            }
        }

        public void Free(Buf<T> buffer)
        {
            foreach(Buf<T> t in _freeList) {
                if (t.Data == buffer.Data) {
                    return;
                }
            }
            _freeList.Add(buffer);
        }
    }
}

