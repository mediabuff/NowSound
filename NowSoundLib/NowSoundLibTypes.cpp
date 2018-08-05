// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include "NowSoundLibTypes.h"

namespace NowSound
{
    NowSoundDeviceInfo CreateNowSoundDeviceInfo(LPWSTR id, LPWSTR name)
    {
        NowSoundDeviceInfo info;
        info.Id = id;
        info.Name = name;
        return info;
    }

    NowSoundGraphInfo CreateNowSoundGraphInfo(
		int32_t latencyInSamples,
		int32_t samplesPerQuantum,
		int64_t timeInSamples,
		float exactBeat,
		float beatsPerMinute,
		float beatInMeasure)
    {
        NowSoundGraphInfo info;
        info.LatencyInSamples = latencyInSamples;
        info.SamplesPerQuantum = samplesPerQuantum;
		info.TimeInSamples = timeInSamples;
        info.ExactBeat = exactBeat;
        info.BeatsPerMinute = beatsPerMinute;
        info.BeatInMeasure = beatInMeasure;
        return info;
    }

	NowSoundInputInfo CreateNowSoundInputInfo(
		float channel0Volume,
		float channel1Volume)
	{
		NowSoundInputInfo info;
		info.Channel0Volume = channel0Volume;
		info.Channel1Volume = channel1Volume;
		return info;
	}

    NowSoundTrackInfo CreateNowSoundTrackInfo(
        int64_t startTimeInSamples,
        float startTimeInBeats,
        int64_t durationInSamples,
        int64_t durationInBeats,
        float exactDuration,
		int64_t localClockTime,
		float localClockBeat,
		int64_t lastSampleTime,
		float recentVolume,
		float minimumRequiredSamples,
        float maximumRequiredSamples,
        float averageRequiredSamples,
        float minimumTimeSinceLastQuantum,
        float maximumTimeSinceLastQuantum,
        float averageTimeSinceLastQuantum)
    {
        NowSoundTrackInfo info;
        info.StartTimeInSamples = startTimeInSamples;
        info.StartTimeInBeats = startTimeInBeats;
        info.DurationInSamples = durationInSamples;
        info.DurationInBeats = durationInBeats;
        info.ExactDuration = exactDuration;
		info.LocalClockTime = localClockTime;
		info.LocalClockBeat = localClockBeat;
		info.LastSampleTime = lastSampleTime;
		info.RecentVolume = recentVolume;
		info.MinimumRequiredSamples = minimumRequiredSamples;
        info.MaximumRequiredSamples = maximumRequiredSamples;
        info.AverageRequiredSamples = averageRequiredSamples;
        info.MinimumTimeSinceLastQuantum = minimumTimeSinceLastQuantum;
        info.MaximumTimeSinceLastQuantum = maximumTimeSinceLastQuantum;
        info.AverageTimeSinceLastQuantum = averageTimeSinceLastQuantum;
        return info;
    }
}
