// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include <algorithm>

#include "Clock.h"
#include "GetBuffer.h"
#include "NowSoundLib.h"
#include "NowSoundGraph.h"
#include "NowSoundTrack.h"

using namespace NowSound;
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
using namespace Windows::System;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;

NowSoundGraphState NowSoundGraphAPI::NowSoundGraph_GetGraphState()
{
    return NowSoundGraph::Instance()->GetGraphState();
}

void NowSoundGraphAPI::NowSoundGraph_InitializeAsync()
{
    NowSoundGraph::Instance()->InitializeAsync();
}

NowSoundDeviceInfo NowSoundGraphAPI::NowSoundGraph_GetDefaultRenderDeviceInfo()
{
    return NowSoundGraph::Instance()->GetDefaultRenderDeviceInfo();
}

void NowSoundGraphAPI::NowSoundGraph_CreateAudioGraphAsync()
{
    return NowSoundGraph::Instance()->CreateAudioGraphAsync();
}

NowSoundGraphInfo NowSoundGraphAPI::NowSoundGraph_GetGraphInfo()
{
    return NowSoundGraph::Instance()->GetGraphInfo();
}

void NowSoundGraphAPI::NowSoundGraph_StartAudioGraphAsync()
{
    NowSoundGraph::Instance()->StartAudioGraphAsync();
}

void NowSoundGraphAPI::NowSoundGraph_PlayUserSelectedSoundFileAsync()
{
    NowSoundGraph::Instance()->PlayUserSelectedSoundFileAsync();
}

void NowSoundGraphAPI::NowSoundGraph_DestroyAudioGraphAsync()
{
    NowSoundGraph::Instance()->DestroyAudioGraphAsync();
}

int NowSoundGraphAPI::NowSoundGraph_CreateRecordingTrackAsync()
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
    _audioGraphState{ NowSoundGraphState::Uninitialized },
    _deviceOutputNode{ nullptr },
    _audioAllocator{ ((int)Clock::SampleRateHz * 2 * sizeof(float)), 128 },
    _audioFrame{ nullptr },
    _defaultInputDevice{ nullptr },
    _inputDeviceFrameOutputNode{ nullptr },
    _trackId{ 0 },
    _recorders{ },
    _recorderMutex{ }
{ }

AudioGraph NowSoundGraph::GetAudioGraph() { return _audioGraph; }

AudioDeviceOutputNode NowSoundGraph::GetAudioDeviceOutputNode() { return _deviceOutputNode; }

BufferAllocator<float>* NowSoundGraph::GetAudioAllocator() { return &_audioAllocator; }

AudioFrame NowSoundGraph::GetAudioFrame() { return _audioFrame; }

NowSoundGraphState NowSoundGraph::GetGraphState() { return _audioGraphState; }

void NowSoundGraph::InitializeAsync()
{
    Check(_audioGraphState == NowSoundGraphState::Uninitialized);
    create_task([&]() -> IAsyncAction
    {
        AudioGraphSettings settings(AudioRenderCategory::Media);
        settings.QuantumSizeSelectionMode(Windows::Media::Audio::QuantumSizeSelectionMode::LowestLatency);
        settings.DesiredRenderDeviceAudioProcessing(Windows::Media::AudioProcessing::Raw);
        // leaving PrimaryRenderDevice uninitialized will use default output device
        CreateAudioGraphResult result = co_await AudioGraph::CreateAsync(settings);

        if (result.Status() != AudioGraphCreationStatus::Success)
        {
            // Cannot create graph
            CoreApplication::Exit();
            return;
        }

        _audioGraph = result.Graph();

        _audioGraphState = NowSoundGraphState::Initialized;

        _audioFrame = AudioFrame(Clock::SampleRateHz / 4 * sizeof(float) * 2);
    });
}

NowSoundDeviceInfo NowSoundGraph::GetDefaultRenderDeviceInfo()
{
    return NowSoundDeviceInfo(nullptr, nullptr);
}

void NowSoundGraph::CreateAudioGraphAsync(/*NowSound_DeviceInfo outputDevice*/) // TODO: output device selection?
{
    // TODO: verify not on audio graph thread

    Check(_audioGraphState == NowSoundGraphState::Initialized);

    create_task([&]() -> IAsyncAction
    {
        // Create a device output node
        CreateAudioDeviceOutputNodeResult deviceOutputNodeResult = co_await _audioGraph.CreateDeviceOutputNodeAsync();

        if (deviceOutputNodeResult.Status() != AudioDeviceNodeCreationStatus::Success)
        {
            // Cannot create device output node
            CoreApplication::Exit();
            return;
        }

        _deviceOutputNode = deviceOutputNodeResult.DeviceOutputNode();

        // Create a device input node
        CreateAudioDeviceInputNodeResult deviceInputNodeResult = co_await 
            _audioGraph.CreateDeviceInputNodeAsync(Windows::Media::Capture::MediaCategory::Media);

        if (deviceInputNodeResult.Status() != AudioDeviceNodeCreationStatus::Success)
        {
            // Cannot create device input node
            CoreApplication::Exit();
            return;
        }

        _defaultInputDevice = deviceInputNodeResult.DeviceInputNode();
        _inputDeviceFrameOutputNode = _audioGraph.CreateFrameOutputNode();
        _defaultInputDevice.AddOutgoingConnection(_inputDeviceFrameOutputNode);

        _audioGraph.QuantumStarted([&](AudioGraph, IInspectable)
        {
            HandleIncomingAudio();
        });

        _audioGraphState = NowSoundGraphState::Created;
    });
}

NowSoundGraphInfo NowSoundGraph::GetGraphInfo()
{
    // TODO: verify not on audio graph thread

    Check(_audioGraphState >= NowSoundGraphState::Created);

    return NowSoundGraphInfo(_audioGraph.LatencyInSamples(), _audioGraph.SamplesPerQuantum());
}

void NowSoundGraph::StartAudioGraphAsync()
{
    // TODO: verify not on audio graph thread

    Check(_audioGraphState == NowSoundGraphState::Created);

    _audioGraph.Start();

    _audioGraphState = NowSoundGraphState::Running;
}

TrackId NowSoundGraph::CreateRecordingTrackAsync()
{
    // TODO: verify not on audio graph thread

    Check(_audioGraphState == NowSoundGraphState::Running);

    TrackId id = _trackId++;

    std::unique_ptr<NowSoundTrack> newTrack(new NowSoundTrack(id, AudioInputId::Input0));

    // new tracks are created as recording; lock the _recorders collection and add this new track
    {
        std::lock_guard<std::mutex> guard(_recorderMutex);
        _recorders.push_back(newTrack.get());
    }

    NowSoundTrackAPI::AddTrack(id, std::move(newTrack));

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
        CoreApplication::Exit();
        return;
    }

    CreateAudioFileInputNodeResult fileInputResult = co_await _audioGraph.CreateFileInputNodeAsync(file);
    if (AudioFileNodeCreationStatus::Success != fileInputResult.Status())
    {
        // Cannot read input file
        CoreApplication::Exit();
        return;
    }

    AudioFileInputNode fileInput = fileInputResult.FileInputNode();

    if (fileInput.Duration() <= timeSpanFromSeconds(3))
    {
        // Imported file is too short
        CoreApplication::Exit();
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
    // TODO: advance the clock

    AudioFrame frame = _inputDeviceFrameOutputNode.GetFrame();

    uint8_t* dataInBytes{};
    uint32_t capacityInBytes{};

    // OMG KENNY KERR WINS AGAIN:
    // https://gist.github.com/kennykerr/f1d941c2d26227abbf762481bcbd84d3
    Windows::Media::AudioBuffer buffer(NowSoundGraph::GetAudioFrame().LockBuffer(Windows::Media::AudioBufferAccessMode::Write));
    IMemoryBufferReference reference(buffer.CreateReference());
    winrt::impl::com_ref<IMemoryBufferByteAccess> interop = reference.as<IMemoryBufferByteAccess>();
    check_hresult(interop->GetBuffer(&dataInBytes, &capacityInBytes));

    if (capacityInBytes == 0)
    {
        // we don't count zero-byte frames... and why do they ever happen???
        return;
    }

    // Must be multiple of 8 (2 channels, 4 bytes/float)
    Check((capacityInBytes & 0x7) == 0);

    Duration<AudioSample> duration(capacityInBytes >> 3);

    uint32_t bufferStart = 0;
    if (Clock::Instance().Now().Value() == 0)
    {
        // if maxCapacityEncountered is greater than the audio graph buffer size, 
        // then the audio graph decided to give us a big backload of buffer content
        // as its first callback.  Not sure why it does this, but we don't want it,
        // so take only the tail of the buffer.
        if (capacityInBytes > ((uint32_t)_audioGraph.LatencyInSamples() << 3))
        {
            bufferStart = capacityInBytes - (uint32_t)(_audioGraph.LatencyInSamples() << 3);
            capacityInBytes = (uint32_t)(_audioGraph.LatencyInSamples() << 3);
        }
    }

    // iterate through all active Recorders
    // note that Recorders must be added or removed only inside the audio graph
    // (e.g. QuantumStarted or FrameInputAvailable)
    std::vector<IRecorder<AudioSample, float>*> _completedRecorders{};
    {
        std::lock_guard<std::mutex> guard(_recorderMutex);
        for (IRecorder<AudioSample, float>* recorder : _recorders)
        {
            bool stillRecording =
                    recorder->Record(Clock::Instance().Now(), duration, (float*)(dataInBytes + bufferStart));
            if (!stillRecording)
            {
                _completedRecorders.push_back(recorder);
            }
        }
        for (IRecorder<AudioSample, float>* completedRecorder : _completedRecorders)
        {
            // not optimally efficient but we will only ever have one or two completed per incoming audio frame
            _recorders.erase(std::find(_recorders.begin(), _recorders.end(), completedRecorder));
        }
    }
}

void NowSoundTrackAPI::AddTrack(TrackId id, std::unique_ptr<NowSoundTrack>&& track)
{
    _tracks.emplace(id, std::move(track));
}
