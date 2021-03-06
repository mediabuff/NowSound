// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "pch.h"

#include "math.h"
#include "stdint.h" // for int64_t

#include "Check.h"

namespace NowSound
{
    // Sample identifies Times based on audio sample counts. 
    // This type is actually never instantiated; it is used purely as a generic type parameter.
    class AudioSample {};

    // Beat identifies times based on beat counts.
    // This type is actually never instantiated; it is used purely as a generic type parameter.
    class Beat {};

    // Seconds identifies times based on real-world seconds.
    // This type is actually never instantiated; it is used purely as a generic type parameter.
    class Second {};

    // Frame identifies Times based on video frame counts. 
    // This type is actually never instantiated; it is used purely as a generic type parameter.
    class Frame {};

    // Time parameterized on some underlying unit of measurement (such as those above).
    // This theoretically could also provide a timing base (e.g. number of units per other-unit),
    // but currently does not.
    // The rationale for these types and the boilerplate around them is that plain numeric types
    // in a multimedia program always lead to confusion -- what is that "int" exactly?  These types
    // eliminate these confusions and help to catch real bugs.
    template<typename TTime>
    class Time
    {
    private:
        // The number of TTime units represented by this time.
        int64_t _value;

    public:
        Time() : _value{} {};

        Time(int64_t value) : _value(value)
        {
        }

        int64_t Value() const { return _value; }

        static Time<TTime> Min(const Time<TTime>& first, const Time<TTime>& second)
        {
            return Time<TTime>(__min(first.Value(), second.Value()));
        }

        static Time<TTime> Max(const Time<TTime>& first, const Time<TTime>& second)
        {
            return Time<TTime>(__max(first.Value(), second.Value()));
        }

        Time<TTime>& operator=(const Time<TTime>& other)
        {
            _value = other.Value();
            return *this;
        }

        bool operator <(const Time<TTime>& second) const
        {
            return _value < second.Value();
        }

        bool operator >(const Time<TTime>& second) const
        {
            return _value > second.Value();
        }

        bool operator ==(const Time<TTime>& second) const
        {
            return _value == second.Value();
        }

        bool operator !=(const Time<TTime>& second) const
        {
            return _value != second.Value();
        }

        bool operator <=(const Time<TTime>& second) const
        {
            return _value <= second.Value();
        }

        bool operator >=(const Time<TTime>& second) const
        {
            return _value >= second.Value();
        }

        double Seconds() const { return ((double)_value.Value()) / Clock::SampleRateHz; }
    };

    // A distance between two Times.
    template<typename TTime>
    class Duration
    {
    private:
        int64_t _value;

    public:
        Duration() : _value{} {};

        Duration(int64_t duration) : _value(duration)
        { }

        int64_t Value() const { return _value; }

        static Duration<TTime> Min(Duration<TTime> first, Duration<TTime> second)
        {
            return new Duration<TTime>(Math.Min(first, second));
        }

        Duration<TTime>& operator=(const Duration<TTime>& other)
        {
            _value = other.Value();
            return *this;
        }

        Duration<TTime> operator /(float second) const
        {
            return Duration<TTime>((int64_t)(_value / second));
        }

        Duration<TTime> operator *(float second) const
        {
            return Duration<TTime>((int64_t)(_value * second));
        }

        Duration<TTime> operator *(int second) const
        {
            return Duration<TTime>(_value * second);
        }

        bool operator <(const Duration<TTime>& second) const
        {
            return _value < second.Value();
        }

        bool operator >(const Duration<TTime>& second) const
        {
            return _value > second.Value();
        }

        bool operator <=(const Duration<TTime>& second) const
        {
            return _value <= second.Value();
        }

        bool operator >=(const Duration<TTime>& second) const
        {
            return _value >= second.Value();
        }

        bool operator ==(const Duration<TTime>& second) const
        {
            return _value == second.Value();
        }

        bool operator !=(const Duration<TTime>& second) const
        {
            return _value != second.Value();
        }
    };

    template<typename TTime>
    Duration<TTime> operator -(Time<TTime> first, Time<TTime> second)
    {
        return Duration<TTime>(first.Value() - second.Value());
    }

    template<typename TTime>
    Time<TTime> operator -(Time<TTime> first, Duration<TTime> second)
    {
        return Time<TTime>(first.Value() - second.Value());
    }

    template<typename TTime>
    Duration<TTime> operator +(Duration<TTime> first, Duration<TTime> second)
    {
        return Duration<TTime>(first.Value() + second.Value());
    }

    template<typename TTime>
    Duration<TTime> operator -(Duration<TTime> first, Duration<TTime> second)
    {
        return Duration<TTime>(first.Value() - second.Value());
    }

    template<typename TTime>
    Time<TTime> operator +(Time<TTime> first, Duration<TTime> second)
    {
        return Time<TTime>(first.Value() + second.Value());
    }

    template<typename TTime>
    Time<TTime> operator +(Duration<TTime> first, Time<TTime> second)
    {
        return Time<TTime>(first._duration + second.Value());
    }

    // An interval, defined as a start time and a duration (aka length).
    // 
    // Empty intervals semantically have no InitialTime, and no distinction should be made between empty
    // intervals based on InitialTime.
    template<typename TTime>
    struct Interval
    {
    private:
        Time<TTime> _initialTime;
        Duration<TTime> _duration;

    public:
        Interval() = delete;

        // Construct the given interval. duration must be non-negative, but initialTime may be any value.
        Interval(Time<TTime> initialTime, Duration<TTime> duration)
            : _initialTime(initialTime), _duration(duration)
        {
            Check(duration >= 0);
        }

        static Interval<TTime> Empty() { return Interval<TTime>(0, 0); }

        Time<TTime> InitialTime() const { return _initialTime; }
        Duration<TTime> IntervalDuration() const { return _duration; }

        bool IsEmpty() const { return _duration.Value() == 0; }

        Interval<TTime> SubintervalStartingAt(Duration<TTime> offset) const
        {
            Check(offset <= _duration);
            return Interval<TTime>(_initialTime + offset, _duration - offset);
        }

        Interval<TTime> SubintervalOfDuration(Duration<TTime> duration) const
        {
            Check(duration <= _duration);
            return Interval<TTime>(_initialTime, duration);
        }

        Interval<TTime> Intersect(const Interval<TTime>& other) const
        {
            Time<TTime> intersectionStart = Time<TTime>::Max(_initialTime, other._initialTime);
            Time<TTime> intersectionEnd = Time<TTime>::Min(_initialTime + _duration, other.InitialTime() + other.IntervalDuration());

            if (intersectionEnd < intersectionStart)
            {
                return Interval<TTime>::Empty();
            }
            else
            {
                Interval<TTime> result(intersectionStart, intersectionEnd - intersectionStart);
                return result;
            }
        }

        bool Contains(Time<TTime> time) const
        {
            if (IsEmpty) 
            {
                return false;
            }

            return _initialTime <= time && (_initialTime + _duration) > time;
        }

        Interval<TTime>& operator=(const Interval<TTime>& other)
        {
            _initialTime = other._initialTime;
            _duration = other._duration;
            return *this;
        }
    };

    // A continous distance between two Times.
    template<typename TTime>
    struct ContinuousDuration
    {
    private:
        float _value;

    public:
        ContinuousDuration() = delete;

        ContinuousDuration(float value) : _value(value)
        {
            Check(value >= 0);
        }

        ContinuousDuration(const ContinuousDuration<TTime>& other) : _value(other.Value())
        {
        }

        float Value() const { return _value; }

        ContinuousDuration<TTime>& operator =(const ContinuousDuration<TTime>& other)
        {
            _value = other._value;
            return *this;
        }

        ContinuousDuration<TTime> operator *(float value) const
        {
            return new ContinuousDuration<TTime>(value * _value);
        }
    };
}
