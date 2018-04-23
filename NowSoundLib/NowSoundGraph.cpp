// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include <algorithm>

#include "Clock.h"
#include "GetBuffer.h"
#include "MagicNumbers.h"
#include "NowSoundLib.h"
#include "NowSoundGraph.h"
#include "NowSoundTrack.h"

using namespace concurrency;
using namespace std;
using namespace std::chrono;
using namespace winrt;

using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;
using namespace Windows::Media;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Render;
using namespace Windows::System;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;

namespace NowSound
{
    NowSoundGraphState NowSoundGraph_GetGraphState()
    {
        return NowSoundGraph::Instance()->GetGraphState();
    }

    void NowSoundGraph_InitializeAsync()
    {
        NowSoundGraph::Instance()->InitializeAsync();
    }

    NowSoundDeviceInfo NowSoundGraph_GetDefaultRenderDeviceInfo()
    {
        return NowSoundGraph::Instance()->GetDefaultRenderDeviceInfo();
    }

    void NowSoundGraph_CreateAudioGraphAsync()
    {
        return NowSoundGraph::Instance()->CreateAudioGraphAsync();
    }

    NowSoundGraphInfo NowSoundGraph_GetGraphInfo()
    {
        return NowSoundGraph::Instance()->GetGraphInfo();
    }

    void NowSoundGraph_StartAudioGraphAsync()
    {
        NowSoundGraph::Instance()->StartAudioGraphAsync();
    }

    NowSoundTimeInfo NowSoundGraph_GetTimeInfo()
    {
        return NowSoundGraph::Instance()->GetTimeInfo();
    }

    void NowSoundGraph_PlayUserSelectedSoundFileAsync()
    {
        NowSoundGraph::Instance()->PlayUserSelectedSoundFileAsync();
    }

    void NowSoundGraph_DestroyAudioGraphAsync()
    {
        NowSoundGraph::Instance()->DestroyAudioGraphAsync();
    }

    int NowSoundGraph_CreateRecordingTrackAsync()
    {
        return NowSoundGraph::Instance()->CreateRecordingTrackAsync();
    }

    TimeSpan timeSpanFromSeconds(int seconds)
    {
        // TimeSpan is in 100ns units
        return TimeSpan(seconds * Clock::TicksPerSecond);
    }

    std::unique_ptr<NowSoundGraph> NowSoundGraph::s_instance{ new NowSoundGraph() };

    NowSoundGraph* NowSoundGraph::Instance() { return s_instance.get(); }

    NowSoundGraph::NowSoundGraph()
        : _audioGraph{ nullptr },
        _audioGraphState{ NowSoundGraphState::GraphUninitialized },
        _deviceOutputNode{ nullptr },
        _audioAllocator{ ((int)Clock::SampleRateHz * MagicNumbers::AudioChannelCount * sizeof(float)), MagicNumbers::InitialAudioBufferCount },
        _audioFrame{ nullptr },
        _defaultInputDevice{ nullptr },
        _inputDeviceFrameOutputNode{ nullptr },
        _trackId{ 0 },
        _recorders{ },
        _recorderMutex{ },
        _changingState{ false },
        _incomingAudioStream(0, MagicNumbers::AudioChannelCount, &_audioAllocator, Clock::SampleRateHz, /*useExactLoopingMapper:*/false),
        _incomingAudioStreamRecorder(&_incomingAudioStream)
    { }

    AudioFrame NowSoundGraph::GetAudioFrame() { return _audioFrame; }

    AudioGraph NowSoundGraph::GetAudioGraph() { return _audioGraph; }

    AudioDeviceOutputNode NowSoundGraph::GetAudioDeviceOutputNode() { return _deviceOutputNode; }

    BufferAllocator<float>* NowSoundGraph::GetAudioAllocator() { return &_audioAllocator; }

    void NowSoundGraph::PrepareToChangeState(NowSoundGraphState expectedState)
    {
        std::lock_guard<std::mutex> guard(_stateMutex);
        Check(_audioGraphState == expectedState);
        Check(!_changingState);
        _changingState = true;
    }

    void NowSoundGraph::ChangeState(NowSoundGraphState newState)
    {
        std::lock_guard<std::mutex> guard(_stateMutex);
        Check(_changingState);
        _changingState = false;
        _audioGraphState = newState;
    }

    NowSoundGraphState NowSoundGraph::GetGraphState()
    {
        std::lock_guard<std::mutex> guard(_stateMutex);
        return _audioGraphState;
    }

    void NowSoundGraph::InitializeAsync()
    {
        PrepareToChangeState(NowSoundGraphState::GraphUninitialized);
        // this does not need to be locked
        create_task([this]() -> IAsyncAction { co_await InitializeAsyncImpl(); });
    }

    IAsyncAction NowSoundGraph::InitializeAsyncImpl()
    {
        AudioGraphSettings settings(AudioRenderCategory::Media);
        settings.QuantumSizeSelectionMode(Windows::Media::Audio::QuantumSizeSelectionMode::LowestLatency);
        settings.DesiredRenderDeviceAudioProcessing(Windows::Media::AudioProcessing::Raw);
        // leaving PrimaryRenderDevice uninitialized will use default output device
        CreateAudioGraphResult result = co_await AudioGraph::CreateAsync(settings);

        if (result.Status() != AudioGraphCreationStatus::Success)
        {
            // Cannot create graph
            Check(false);
            return;
        }

        // NOTE that if this logic is inlined into the create_task lambda in InitializeAsync,
        // this assignment blows up saying that it is assigning to a value of 0xFFFFFFFFFFFF.
        // Probable compiler bug?  TODO: replicate the bug in test app.
        _audioGraph = result.Graph();

        ChangeState(NowSoundGraphState::GraphInitialized);
    }

    NowSoundDeviceInfo NowSoundGraph::GetDefaultRenderDeviceInfo()
    {
        return CreateNowSoundDeviceInfo(nullptr, nullptr);
    }

    void NowSoundGraph::CreateAudioGraphAsync(/*NowSound_DeviceInfo outputDevice*/) // TODO: output device selection?
    {
        // TODO: verify not on audio graph thread

        PrepareToChangeState(NowSoundGraphState::GraphInitialized);
        create_task([this]() -> IAsyncAction { co_await CreateAudioGraphAsyncImpl(); });
    }

    IAsyncAction NowSoundGraph::CreateAudioGraphAsyncImpl()
    {
        // Create a device output node
        CreateAudioDeviceOutputNodeResult deviceOutputNodeResult = co_await _audioGraph.CreateDeviceOutputNodeAsync();

        if (deviceOutputNodeResult.Status() != AudioDeviceNodeCreationStatus::Success)
        {
            // Cannot create device output node
            Check(false);
            return;
        }

        _deviceOutputNode = deviceOutputNodeResult.DeviceOutputNode();

        // Create a device input node
        CreateAudioDeviceInputNodeResult deviceInputNodeResult = co_await
            _audioGraph.CreateDeviceInputNodeAsync(Windows::Media::Capture::MediaCategory::Media);

        auto deviceInputNodeResultStatus = deviceInputNodeResult.Status();
        if (deviceInputNodeResultStatus != AudioDeviceNodeCreationStatus::Success)
        {
            // Cannot create device input node
            Check(false);
            return;
        }

        _defaultInputDevice = deviceInputNodeResult.DeviceInputNode();
        _inputDeviceFrameOutputNode = _audioGraph.CreateFrameOutputNode();
        _defaultInputDevice.AddOutgoingConnection(_inputDeviceFrameOutputNode);

        _audioGraph.QuantumStarted([&](AudioGraph, IInspectable)
        {
            HandleIncomingAudio();
        });

        ChangeState(NowSoundGraphState::GraphCreated);
    }

    NowSoundGraphInfo NowSoundGraph::GetGraphInfo()
    {
        // TODO: verify not on audio graph thread

        Check(_audioGraphState >= NowSoundGraphState::GraphCreated);

        NowSoundGraphInfo graphInfo = CreateNowSoundGraphInfo(_audioGraph.LatencyInSamples(), _audioGraph.SamplesPerQuantum());
        return graphInfo; // TODO: why does this fail in Holofunk with "cannot write location 0x0000000000"???)
    }

    void NowSoundGraph::StartAudioGraphAsync()
    {
        // TODO: verify not on audio graph thread

        PrepareToChangeState(NowSoundGraphState::GraphCreated);

        // MAKE THE CLOCK NOW.
        Clock::Initialize(MagicNumbers::InitialBeatsPerMinute, MagicNumbers::BeatsPerMeasure, MagicNumbers::AudioChannelCount);

        // Add the input stream recorder (don't need to lock _recorders quiiiite yet...)
        _recorders.push_back(&_incomingAudioStreamRecorder);

        // not actually async!  But let's not expose that, maybe this might be async later or we might add async stuff here.
        _audioGraph.Start();

        // As of now, we will start getting HandleIncomingAudio() callbacks.

        ChangeState(NowSoundGraphState::GraphRunning);
    }

    NowSoundTimeInfo NowSoundGraph::GetTimeInfo()
    {
        // TODO: verify not on audio graph thread

        Check(_audioGraphState == NowSoundGraphState::GraphRunning);

        Time<AudioSample> now = Clock::Instance().Now();
        int64_t completeBeats = Clock::Instance().TimeToCompleteBeats(now).Value();
        return CreateNowSoundTimeInfo(
            now.Value(),
            Clock::Instance().TimeToBeats(now).Value(),
            Clock::Instance().BeatsPerMinute(),
            completeBeats % Clock::Instance().BeatsPerMeasure());
    }

    TrackId NowSoundGraph::CreateRecordingTrackAsync()
    {
        // TODO: verify not on audio graph thread

        Check(_audioGraphState == NowSoundGraphState::GraphRunning);

        TrackId id = _trackId++;

        std::unique_ptr<NowSoundTrack> newTrack(new NowSoundTrack(id, AudioInputId::Input0, _incomingAudioStream));

        // new tracks are created as recording; lock the _recorders collection and add this new track
        {
            std::lock_guard<std::mutex> guard(_recorderMutex);
            _recorders.push_back(newTrack.get());
        }

        // move the new track over to the collection of tracks in NowSoundTrackAPI
        NowSoundTrack::AddTrack(id, std::move(newTrack));

        return id;
    }

    IAsyncAction NowSoundGraph::PlayUserSelectedSoundFileAsyncImpl()
    {
        // This must be called on the UI thread.
        FileOpenPicker picker;
        picker.SuggestedStartLocation(PickerLocationId::MusicLibrary);
        picker.FileTypeFilter().Append(L".wav");
        StorageFile file = co_await picker.PickSingleFileAsync();

        if (!file)
        {
            Check(false);
            return;
        }

        CreateAudioFileInputNodeResult fileInputResult = co_await _audioGraph.CreateFileInputNodeAsync(file);
        if (AudioFileNodeCreationStatus::Success != fileInputResult.Status())
        {
            // Cannot read input file
            Check(false);
            return;
        }

        AudioFileInputNode fileInput = fileInputResult.FileInputNode();

        if (fileInput.Duration() <= timeSpanFromSeconds(3))
        {
            // Imported file is too short
            Check(false);
            return;
        }

        fileInput.AddOutgoingConnection(_deviceOutputNode);
        fileInput.Start();
    }

    void NowSoundGraph::PlayUserSelectedSoundFileAsync()
    {
        PlayUserSelectedSoundFileAsyncImpl();
    }

    void NowSoundGraph::DestroyAudioGraphAsync()
    {
    }

    void NowSoundGraph::HandleIncomingAudio()
    {
        if (_audioFrame == nullptr)
        {
            // The AudioFrame.Duration property is a TimeSpan, despite the fact that this seems an inherently
            // inaccurate way to precisely express an audio sample count.  So we just have a short frame and
            // we fill it completely and often.
            _audioFrame = AudioFrame(
                (uint32_t)(Clock::SampleRateHz
                    * MagicNumbers::AudioFrameLengthSeconds.Value()
                    * sizeof(float)
                    * MagicNumbers::AudioChannelCount));
        }

        AudioFrame frame = _inputDeviceFrameOutputNode.GetFrame();

        uint8_t* dataInBytes{};
        uint32_t capacityInBytes{};

        // OMG KENNY KERR WINS AGAIN:
        // https://gist.github.com/kennykerr/f1d941c2d26227abbf762481bcbd84d3
        Windows::Media::AudioBuffer buffer(frame.LockBuffer(Windows::Media::AudioBufferAccessMode::Read));
        IMemoryBufferReference reference(buffer.CreateReference());
        winrt::impl::com_ref<IMemoryBufferByteAccess> interop = reference.as<IMemoryBufferByteAccess>();
        check_hresult(interop->GetBuffer(&dataInBytes, &capacityInBytes));

        if (capacityInBytes == 0)
        {
            // we don't count zero-byte frames... and why do they ever happen???
            return;
        }

        // Must be multiple of channels * sizeof(float)
        int sampleSizeInBytes = MagicNumbers::AudioChannelCount * sizeof(float);
        Check((capacityInBytes & (sampleSizeInBytes - 1)) == 0);

        uint32_t bufferStart = 0;
        if (Clock::Instance().Now().Value() == 0)
        {
            // if maxCapacityEncountered is greater than the audio graph buffer size, 
            // then the audio graph decided to give us a big backload of buffer content
            // as its first callback.  Not sure why it does this, but we don't want it,
            // so take only the tail of the buffer.
            uint32_t latencyInSamples = ((uint32_t)_audioGraph.LatencyInSamples() * sampleSizeInBytes);
            if (latencyInSamples == 0)
            {
                // sorry audiograph, don't really believe you when you say zero latency.
                latencyInSamples = (int)(Clock::SampleRateHz * MagicNumbers::AudioFrameLengthSeconds.Value());
            }
            if (capacityInBytes > latencyInSamples)
            {
                bufferStart = capacityInBytes - (uint32_t)(_audioGraph.LatencyInSamples() * sampleSizeInBytes);
                capacityInBytes = (uint32_t)(_audioGraph.LatencyInSamples() * sampleSizeInBytes);
            }
        }

        Duration<AudioSample> duration(capacityInBytes / sampleSizeInBytes);

        Clock::Instance().AdvanceFromAudioGraph(duration);

        // iterate through all active Recorders
        // note that Recorders must be added or removed only inside the audio graph
        // (e.g. QuantumStarted or FrameInputAvailable)
        std::vector<IRecorder<AudioSample, float>*> _completedRecorders{};
        {
            std::lock_guard<std::mutex> guard(_recorderMutex);

            // Give the new audio to each Recorder, collecting the ones that are done.
            for (IRecorder<AudioSample, float>* recorder : _recorders)
            {
                bool stillRecording =
                    recorder->Record(duration, (float*)(dataInBytes + bufferStart));

                if (!stillRecording)
                {
                    _completedRecorders.push_back(recorder);
                }
            }

            // Now remove all the done ones.
            for (IRecorder<AudioSample, float>* completedRecorder : _completedRecorders)
            {
                // not optimally efficient but we will only ever have one or two completed per incoming audio frame
                _recorders.erase(std::find(_recorders.begin(), _recorders.end(), completedRecorder));
            }
        }
    }
}
