// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "pch.h"

#include "BufferAllocator.h"
#include "Time.h"

namespace NowSound
{
    // A reference to a sub-segment of an underlying buffer, indexed by the given TTime type.
    // A Slice is a contiguous segment of Slivers; think of each Sliver as a stereo pair of audio samples,
    // or a video frame, etc., with a Slice being a logically and physically continuous sequence
    // thereof.
    template<typename TTime, typename TValue>
    class Slice
    {
    private:
        // The backing store; logically divided into slivers.
        // This is borrowed from this slice's containing stream.
        const Buf<TValue>* _buffer;

        // Copy memory from src to dest, using byte offsets.
        static void ArrayCopy(const TValue* src, int64_t srcOffset, const TValue* dest, int64_t destOffset, int64_t length)
        {
            std::memcpy(
                (void*)dest + destOffset,
                (void*)src + srcOffset,
                length);
        }

    public:
        // The number of slivers contained.
        const Duration<TTime> _duration;

        // The index to the sliver at which this slice actually begins.
        const Duration<TTime> _offset;

        // The size of each sliver in this slice; a count of T.
        // Slices are composed of multiple Slivers, one per unit of Duration.
        const int _sliverSize;

        Slice(Buf<TValue>* buffer, Duration<TTime> offset, Duration<TTime> duration, int sliverSize)
            : _buffer(buffer), _offset(offset), _duration(duration), _sliverSize(sliverSize)
        {
            Check(buffer.Data != null);
            Check(offset >= 0);
            Check(duration >= 0);
            Check((offset * sliverSize) + (duration * sliverSize) <= buffer.Data.Length);
        }

        Slice(Buf<TValue>* buffer, int sliverSize)
            : _buffer(buffer), _offset(0), _duration(buffer.Data.Length / sliverSize), _sliverSize(sliverSize)
        {
        }

        bool IsEmpty() { return Duration == 0; }

        // For use by extension methods only
        const Buf<TValue>* Buffer() { return _buffer; }

        // Get a single value out of the slice at the given offset, sub-indexed in the slice by the given sub-index.
        TValue& Get(Duration<TTime> offset, int subindex)
        {
            Duration<TTime> totalOffset = _offset + offset;
            Check(totalOffset * _sliverSize < Buffer.Data.Length);
            long finalOffset = totalOffset * _sliverSize + subindex;
            return _buffer.Data[finalOffset];
        }

        // Get a portion of this Slice, starting at the given offset, for the given duration.
        Slice<TTime, TValue> Subslice(Duration<TTime> initialOffset, Duration<TTime> duration)
        {
            Check(initialOffset >= 0); // can't slice before the beginning of this slice
            Check(_duration >= 0); // must be nonnegative count
            Check(initialOffset + duration <= _duration); // can't slice beyond the end
            return new Slice<TTime, TValue>(_buffer, Offset + initialOffset, duration, _sliverSize);
        }

        // Get the rest of this Slice starting at the given offset.
        Slice<TTime, TValue> SubsliceStartingAt(Duration<TTime> initialOffset)
        {
            return Subslice(initialOffset, _duration - initialOffset);
        }

        TValue* OffsetPointer() { return Buffer()->Data + (_offset * _sliverSize); }

        // Get the prefix of this Slice starting at offset 0 and extending for the requested duration.
        Slice<TTime, TValue> SubsliceOfDuration(Duration<TTime> duration)
        {
            return Subslice(0, duration);
        }

        size_t SliverSizeInBytes() { return sizeof(TValue) * _sliverSize; }
        size_t SizeInBytes() { return SliverSizeInBytes() * _duration.Value(); }

        // Copy this slice's data into destination; destination must be long enough.
        void CopyTo(Slice<TTime, TValue>& destination) const
        {
            Check(destination._duration >= _duration);
            Check(destination._sliverSize == _sliverSize);

            // TODO: support reversed copies etc.
            ArrayCopy(_buffer.Data,
                _offset.Value() * _sliverSize,
                destination._buffer.Data,
                destination._offset.Value() * _sliverSize,
                SizeInBytes());
        }

        void CopyTo(TValue* dest) const
        {
            ArrayCopy(_buffer->Data, _offset.Value() * _sliverSize, dest, 0, SizeInBytes());
        }

        // Copy data from the source, replacing all data in this slice.
        void CopyFrom(TValue* source)
        {
            ArrayCopy(source, 0, _buffer.Data, _offset * _sliverSize, SizeInBytes());
        }

        // Are these samples adjacent in their underlying storage?
        bool Precedes(const Slice<TTime, TValue>& next)
        {
            return _buffer.Data == next._buffer.Data && _offset + _duration == next._offset;
        }

        // Merge two adjacent samples into a single sample.
        // Precedes(next) must be true.
        Slice<TTime, TValue> UnionWith(const Slice<TTime, TValue>& next) const
        {
            Check(Precedes(next));
            return Slice<TTime, TValue>(_buffer, _offset, _duration + next._duration, _sliverSize);
        }

        // Equality comparison.
        bool Equals(const Slice<TTime, TValue>& other)
        {
            return Buffer.Equals(other.Buffer) && Offset == other.Offset && _duration == other._duration;
        }
    };

    // A slice with an absolute initial time associated with it.
    // 
    // In the case of BufferedStreams, the first TimedSlice's InitialTime will be the InitialTime of the stream itself.
    template<typename TTime, typename TValue>
    struct TimedSlice
    {
    private:
        const Slice<TTime, TValue> _value;

    public:
        const Time<TTime> InitialTime;

        const Slice<TTime, TValue>& Value() { return _value; }

        TimedSlice(Time<TTime> startTime, Slice<TTime, TValue> slice) : InitialTime(startTime), _value(slice)
        {
        }

        Interval<TTime> SliceInterval() { return new Interval<TTime>(InitialTime, _value._duration); }
    };
}
