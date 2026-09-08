#include <AudioToolbox/AudioToolbox.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

#include <radsound_hal.hpp>
#include <radsoundobject.hpp>
#include <radmemory.hpp>
#include <raddebug.hpp>
#include <radtime.hpp>

#include "../../../upstream/game/libs/radsound/src/hal/common/audioformat.hpp"
#include "../../../upstream/game/libs/radsound/src/hal/common/memoryregion.hpp"
#include "../../../upstream/game/libs/radsound/src/hal/common/softwarelistener.hpp"
#include "../../../upstream/game/libs/radsound/src/common/radsoundupdatableobject.hpp"

namespace
{
bool SoundTraceEnabled() { static const bool enabled = std::getenv("HMR_SOUND_TRACE") != nullptr; return enabled; }
}

namespace
{
constexpr unsigned kOutputRate = 48000;
constexpr unsigned kOutputChannels = 2;

class MacVoice;

class MacBuffer final : public IRadSoundHalBuffer, public IRadSoundHalDataSourceCallback, public radSoundObject
{
public:
    IMPLEMENT_REFCOUNTED("MacBuffer")
    MacBuffer() : mFrames(0), mLooping(false), mStreaming(false), mLoadCallback(nullptr) {}
    ~MacBuffer() override = default;

    void Initialize(IRadSoundHalAudioFormat* format, IRadMemoryObject* memory, unsigned frames, bool looping, bool streaming) override
    {
        mFormat = format;
        mMemory = memory;
        mFrames = frames;
        mLooping = looping;
        mStreaming = streaming;
        mFramesEverLoaded = 0;
        mPendingLoadStart = 0;
        mPendingLoadFrames = 0;
        // radMemoryAllocAligned intentionally returns uninitialised memory.
        // Audio may start before an async RSD read completes, so every buffer
        // must begin as PCM silence rather than leaking arbitrary memory as a
        // permanent hiss/crackle into the output queue.
        if (mMemory && mFormat && mFrames)
        {
            const unsigned char silence = mFormat->GetBitResolution() == 8 ? 128 : 0;
            std::memset(mMemory->GetMemoryAddress(), silence, mFormat->FramesToBytes(mFrames));
        }
    }
    IRadSoundHalAudioFormat* GetFormat() override { return mFormat; }
    IRadMemoryObject* GetMemoryObject() override { return mMemory; }
    bool IsLooping() override { return mLooping; }
    unsigned GetSizeInFrames() override { return mFrames; }
    // Non-streaming clips arrive fully loaded before Play() is ever called
    // (the generic radsound clip player only calls Play() once its single
    // LoadAsync has completed), so they are always safe to read in full.
    // Streaming buffers are a ring that Play() can start consuming from
    // while radSoundStreamPlayer is still priming it a chunk at a time;
    // reading past what has actually landed in memory yields silence
    // forever (the read cursor keeps wrapping through never-written
    // memory). Track how much of the ring has been touched at least once
    // so Mix() can stall -- instead of reading garbage -- until playback
    // has genuinely caught up to real data.
    bool IsPrimed(unsigned frame) const { return !mStreaming || mFramesEverLoaded >= mFrames || frame < mFramesEverLoaded; }
    void LoadAsync(IRadSoundHalDataSource* source, unsigned start, unsigned frames, IRadSoundHalBufferLoadCallback* callback) override
    {
        if (!mMemory || !mFormat || !source) { if (callback) callback->OnBufferLoadComplete(0); return; }
        mLoadCallback = callback;
        const unsigned clampedStart = std::min(start, mFrames);
        const unsigned clampedFrames = std::min(frames, mFrames - clampedStart);
        mPendingLoadStart = clampedStart;
        mPendingLoadFrames = clampedFrames;
        source->GetFramesAsync(static_cast<char*>(mMemory->GetMemoryAddress()) + mFormat->FramesToBytes(clampedStart), radMemorySpace_Local, clampedFrames, this);
    }
    void ClearAsync(unsigned start, unsigned frames, IRadSoundHalBufferClearCallback* callback) override
    {
        if (mMemory && mFormat && start < mFrames)
        {
            const unsigned count = std::min(frames, mFrames - start);
            const unsigned char silence = mFormat->GetBitResolution() == 8 ? 128 : 0;
            std::memset(static_cast<char*>(mMemory->GetMemoryAddress()) + mFormat->FramesToBytes(start), silence, mFormat->FramesToBytes(count));
        }
        if (callback) callback->OnBufferClearComplete();
    }
    unsigned GetMinTransferSize(IRadSoundHalAudioFormat::SizeType type) override
    { return mFormat ? mFormat->ConvertSizeType(type, 1, IRadSoundHalAudioFormat::Frames) : 1; }
    void CancelAsyncOperations() override { mLoadCallback = nullptr; }
    void ReSetAudioFormat(IRadSoundHalAudioFormat* format) override { mFormat = format; }
    void OnDataSourceFramesLoaded(unsigned frames) override
    {
        IRadSoundHalBufferLoadCallback* callback = mLoadCallback;
        mLoadCallback = nullptr;
        // frames may be less than requested (short read); only credit the
        // portion actually written, tracked from the front of the ring so
        // that a still-priming stream's read cursor only ever advances
        // into memory that genuinely holds decoded audio.
        const unsigned actualFrames = std::min(frames, mPendingLoadFrames);
        const unsigned actualEnd = std::min(mFrames, mPendingLoadStart + actualFrames);
        mFramesEverLoaded = std::min(mFrames, std::max(mFramesEverLoaded, actualEnd));
        if (SoundTraceEnabled())
        {
            rReleasePrintf( "macOS sound: OnDataSourceFramesLoaded frames=%u streaming=%d loaded=%u/%u\n",
                             frames, mStreaming ? 1 : 0, mFramesEverLoaded, mFrames );
        }
        if (callback) callback->OnBufferLoadComplete(frames);
    }
private:
    ref<IRadSoundHalAudioFormat> mFormat;
    ref<IRadMemoryObject> mMemory;
    unsigned mFrames;
    bool mLooping;
    bool mStreaming;
    unsigned mFramesEverLoaded = 0;
    unsigned mPendingLoadStart = 0;
    unsigned mPendingLoadFrames = 0;
    ref<IRadSoundHalBufferLoadCallback> mLoadCallback;
};

class MacSystem;

class MacVoice final : public IRadSoundHalVoice, public radSoundObject
{
public:
    IMPLEMENT_REFCOUNTED("MacVoice")
    explicit MacVoice(MacSystem* system);
    ~MacVoice() override;
    void SetPriority(unsigned priority) override { mPriority = priority; }
    unsigned GetPriority() override { return mPriority; }
    void SetBuffer(IRadSoundHalBuffer* buffer) override { mBuffer = static_cast<MacBuffer*>(buffer); mPosition = 0.0; }
    IRadSoundHalBuffer* GetBuffer() override { return mBuffer; }
    void Play() override
    {
        mPlaying = mBuffer != nullptr;
        mLoggedThisPlay = false;
        if (SoundTraceEnabled())
        {
            rReleasePrintf( "macOS sound: MacVoice::Play() buffer=%p volume=%.3f trim=%.3f pan=%.3f muted=%d aux0=%.3f/%d aux1=%.3f/%d\n",
                             static_cast<void*>(static_cast<MacBuffer*>(mBuffer)),
                             mVolume, mTrim, mPan, mMuted ? 1 : 0,
                             mAuxGain[0], static_cast<int>(mAuxMode[0]),
                             mAuxGain[1], static_cast<int>(mAuxMode[1]) );
        }
    }
    void Stop() override { mPlaying = false; }
    bool IsPlaying() override { return mPlaying; }
    void SetPlaybackPositionInSamples(unsigned position) override { mPosition = mBuffer && mBuffer->GetFormat() ? mBuffer->GetFormat()->SamplesToFrames(position) : 0; }
    unsigned GetPlaybackPositionInSamples() override { return mBuffer && mBuffer->GetFormat() ? mBuffer->GetFormat()->FramesToSamples(static_cast<unsigned>(mPosition)) : 0; }
    void SetVolume(float value) override { mVolume = value; }
    float GetVolume() override { return mVolume; }
    void SetTrim(float value) override { mTrim = value; }
    float GetTrim() override { return mTrim; }
    void SetMuted(bool value) override { mMuted = value; }
    bool GetMuted() override { return mMuted; }
    void SetPan(float value) override { mPan = std::max(-1.0f, std::min(1.0f, value)); }
    float GetPan() override { return mPan; }
    void SetPitch(float value) override { mPitch = std::max(0.01f, value); }
    float GetPitch() override { return mPitch; }
    void SetAuxMode(unsigned aux, radSoundAuxMode mode) override { if (aux < 2) mAuxMode[aux] = mode; }
    radSoundAuxMode GetAuxMode(unsigned aux) override { return aux < 2 ? mAuxMode[aux] : radSoundAuxMode_Off; }
    void SetAuxGain(unsigned aux, float gain) override { if (aux < 2) mAuxGain[aux] = gain; }
    float GetAuxGain(unsigned aux) override { return aux < 2 ? mAuxGain[aux] : 0.0f; }
    void SetPositionalGroup(IRadSoundHalPositionalGroup* group) override { mPositionalGroup = group; }
    IRadSoundHalPositionalGroup* GetPositionalGroup() override { return mPositionalGroup; }
    void Mix(float* output, unsigned frames);
private:
    MacSystem* mSystem;
    ref<MacBuffer> mBuffer;
    ref<IRadSoundHalPositionalGroup> mPositionalGroup;
    unsigned mPriority = 0;
    double mPosition = 0.0;
    float mVolume = 1.0f, mTrim = 1.0f, mPan = 0.0f, mPitch = 1.0f;
    bool mPlaying = false, mMuted = false;
    bool mWasStalled = false;
    bool mLoggedThisPlay = false;
    radSoundAuxMode mAuxMode[2] = {radSoundAuxMode_Off, radSoundAuxMode_Off};
    float mAuxGain[2] = {0.0f, 0.0f};
};

class MacSystem final : public IRadSoundHalSystem, public radSoundObject
{
public:
    IMPLEMENT_REFCOUNTED("MacSystem")
    MacSystem() = default;
    ~MacSystem() override { ShutdownQueue(); if (mSoundMemory) radMemoryFreeAligned(GetThisAllocator(), mSoundMemory); if (radSoundHalMemoryRegion::GetRootRegion()) radSoundHalMemoryRegion::Terminate(); radSoundHalListener::Terminate(); }
    void Initialize(const SystemDescription& description) override
    {
        mNumAux = std::min(description.m_NumAuxSends, 2u);
        mSoundMemory = radMemoryAllocAligned(GetThisAllocator(), description.m_ReservedSoundMemory, radSoundHalDataSourceReadAlignmentGet());
        radSoundHalMemoryRegion::Initialize(mSoundMemory, description.m_ReservedSoundMemory, description.m_MaxRootAllocations, radSoundHalDataSourceReadAlignmentGet(), radMemorySpace_Local, GetThisAllocator());
        radSoundHalListener::Initialize(GetThisAllocator());
        AudioStreamBasicDescription format = {};
        format.mSampleRate = kOutputRate; format.mFormatID = kAudioFormatLinearPCM; format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        format.mChannelsPerFrame = kOutputChannels; format.mBitsPerChannel = 32; format.mFramesPerPacket = 1;
        format.mBytesPerFrame = sizeof(float) * kOutputChannels; format.mBytesPerPacket = format.mBytesPerFrame;
        if (AudioQueueNewOutput(&format, QueueCallback, this, nullptr, nullptr, 0, &mQueue) == noErr)
        {
            for (unsigned i = 0; i < 3; ++i) { AudioQueueBufferRef buffer = nullptr; AudioQueueAllocateBuffer(mQueue, 2048 * format.mBytesPerFrame, &buffer); QueueCallback(this, mQueue, buffer); }
            AudioQueueStart(mQueue, nullptr);
        }
    }
    IRadSoundHalMemoryRegion* GetRootMemoryRegion() override { return radSoundHalMemoryRegion::GetRootRegion(); }
    unsigned GetNumAuxSends() override { return mNumAux; }
    void SetAuxEffect(unsigned aux, IRadSoundHalEffect* effect) override { if (aux < 2) mEffects[aux] = effect; }
    IRadSoundHalEffect* GetAuxEffect(unsigned aux) override { return aux < 2 ? mEffects[aux] : nullptr; }
    void SetAuxGain(unsigned aux, float gain) override { if (aux < 2) mAuxGain[aux] = gain; }
    float GetAuxGain(unsigned aux) override { return aux < 2 ? mAuxGain[aux] : 0.0f; }
    void SetOutputMode(radSoundOutputMode) override {}
    radSoundOutputMode GetOutputMode() override { return radSoundOutputMode_Stereo; }
    void Service() override
    {
        // Every other platform's HAL (win32/xbox/ps2/gcn) drives
        // radSoundUpdatableObject::UpdateAll() from here. Without it,
        // nothing built on radSoundClip/radSoundStreamPlayer (radmusic's
        // resource manager, in particular) ever progresses past its first
        // "kick start" tick: file-open completions and buffer loads just
        // sit there, so streamed music/dialogue silently never finishes
        // "loading" and never plays, while directly-buffered SFX clips
        // (which don't go through this update list) are unaffected.
        const unsigned int now = ::radTimeGetMilliseconds();
        radSoundUpdatableObject::UpdateAll( now - mLastServiceTime );
        mLastServiceTime = now;
    }
    void ServiceOncePerFrame() override { if (radSoundHalListener::GetInstance()) radSoundHalListener::GetInstance()->UpdatePositionalSettings(); }
    void GetStats(Stats* stats) override
    {
        std::memset(stats, 0, sizeof(*stats)); std::lock_guard<std::mutex> lock(mMutex);
        for (MacVoice* voice : mVoices) { ++stats->m_NumVoices; if (voice->IsPlaying()) ++stats->m_NumVoicesPlaying; }
        if (GetRootMemoryRegion()) GetRootMemoryRegion()->GetStats(&stats->m_TotalFreeSoundMemory, nullptr, nullptr, true);
    }
    void AddVoice(MacVoice* voice) { std::lock_guard<std::mutex> lock(mMutex); mVoices.push_back(voice); }
    void RemoveVoice(MacVoice* voice) { std::lock_guard<std::mutex> lock(mMutex); mVoices.erase(std::remove(mVoices.begin(), mVoices.end(), voice), mVoices.end()); }
private:
    static void QueueCallback(void* data, AudioQueueRef queue, AudioQueueBufferRef buffer)
    {
        auto* self = static_cast<MacSystem*>(data); float* samples = static_cast<float*>(buffer->mAudioData);
        const unsigned frames = buffer->mAudioDataBytesCapacity / (sizeof(float) * kOutputChannels);
        std::memset(samples, 0, frames * sizeof(float) * kOutputChannels);
        { std::lock_guard<std::mutex> lock(self->mMutex); for (MacVoice* voice : self->mVoices) voice->Mix(samples, frames); }
        // AudioQueue accepts floating point samples but does not protect us
        // from the old mixer summing many simultaneous effects beyond the
        // valid [-1, 1] range.  That overflow manifests as a constant harsh
        // crackle on modern macOS output devices.  Soft-limit once per mixed
        // sample, preserving normal volume while preventing digital clipping.
        for (unsigned sample = 0; sample < frames * kOutputChannels; ++sample)
            samples[sample] = std::clamp(samples[sample], -1.0f, 1.0f);
        buffer->mAudioDataByteSize = frames * sizeof(float) * kOutputChannels;
        AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
    }
    void ShutdownQueue() { if (mQueue) { AudioQueueStop(mQueue, true); AudioQueueDispose(mQueue, true); mQueue = nullptr; } }
    AudioQueueRef mQueue = nullptr;
    std::mutex mMutex;
    std::vector<MacVoice*> mVoices;
    void* mSoundMemory = nullptr;
    unsigned mNumAux = 0;
    unsigned int mLastServiceTime = 0;
    ref<IRadSoundHalEffect> mEffects[2];
    float mAuxGain[2] = {1.0f, 1.0f};
};

MacSystem* gSystem = nullptr;
MacVoice::MacVoice(MacSystem* system) : mSystem(system) { if (mSystem) mSystem->AddVoice(this); }
MacVoice::~MacVoice() { if (mSystem) mSystem->RemoveVoice(this); }
void MacVoice::Mix(float* output, unsigned outputFrames)
{
    if (!mPlaying || mMuted || !mBuffer || !mBuffer->GetMemoryObject() || !mBuffer->GetFormat()) return;
    IRadSoundHalAudioFormat* format = mBuffer->GetFormat();
    if (format->GetEncoding() != IRadSoundHalAudioFormat::PCM) return;
    const unsigned channels = format->GetNumberOfChannels(), bits = format->GetBitResolution(), size = mBuffer->GetSizeInFrames();
    if (size == 0 || (bits != 8 && bits != 16) || channels == 0) return;
    const float gain = mVolume * mTrim; const float left = gain * (mPan > 0 ? 1.0f - mPan : 1.0f); const float right = gain * (mPan < 0 ? 1.0f + mPan : 1.0f);
    const double step = (static_cast<double>(format->GetSampleRate()) / kOutputRate) * mPitch;
    const unsigned char* bytes = static_cast<const unsigned char*>(mBuffer->GetMemoryObject()->GetMemoryAddress());
    for (unsigned out = 0; out < outputFrames; ++out)
    {
        unsigned frame = static_cast<unsigned>(mPosition);
        if (frame >= size) { if (mBuffer->IsLooping()) { mPosition = std::fmod(mPosition, static_cast<double>(size)); frame = static_cast<unsigned>(mPosition); } else { mPlaying = false; break; } }
        // A streaming buffer's ring can still be mid-fill: stall (leave
        // this and later output frames silent, don't advance the read
        // cursor) rather than play back not-yet-decoded memory, which
        // would otherwise sound like permanent silence once the cursor
        // wraps back over the same never-written region forever.
        if (!mBuffer->IsPrimed(frame))
        {
            if (SoundTraceEnabled() && !mWasStalled)
            {
                rReleasePrintf( "macOS sound: Mix() stalled waiting for stream fill at frame=%u\n", frame );
                mWasStalled = true;
            }
            break;
        }
        if (mWasStalled)
        {
            if (SoundTraceEnabled()) rReleasePrintf( "macOS sound: Mix() resumed at frame=%u\n", frame );
            mWasStalled = false;
        }
        if (SoundTraceEnabled() && !mLoggedThisPlay)
        {
            rReleasePrintf( "macOS sound: Mix() producing audio buffer=%p gainL=%.4f gainR=%.4f rate=%u channels=%u bits=%u aux0=%.3f/%d aux1=%.3f/%d\n",
                             static_cast<void*>(static_cast<MacBuffer*>(mBuffer)), left, right,
                             format->GetSampleRate(), channels, bits,
                             mAuxGain[0], static_cast<int>(mAuxMode[0]),
                             mAuxGain[1], static_cast<int>(mAuxMode[1]) );
            mLoggedThisPlay = true;
        }
        float l, r;
        if (bits == 16) { const int16_t* source = reinterpret_cast<const int16_t*>(bytes) + frame * channels; l = source[0] / 32768.0f; r = channels > 1 ? source[1] / 32768.0f : l; }
        else { const uint8_t* source = bytes + frame * channels; l = (source[0] - 128) / 128.0f; r = channels > 1 ? (source[1] - 128) / 128.0f : l; }
        output[out * 2] += l * left; output[out * 2 + 1] += r * right; mPosition += step;
    }
}
}

void radSoundHalSystemInitialize(radMemoryAllocator allocator) { if (!gSystem) { gSystem = new("MacSystem", allocator) MacSystem; gSystem->AddRef(); } }
void radSoundHalSystemTerminate() { if (gSystem) { gSystem->Release(); gSystem = nullptr; } }
IRadSoundHalSystem* radSoundHalSystemGet() { return gSystem; }
IRadSoundHalVoice* radSoundHalVoiceCreate(radMemoryAllocator allocator) { return new("MacVoice", allocator) MacVoice(gSystem); }
IRadSoundHalBuffer* radSoundHalBufferCreate(radMemoryAllocator allocator) { return new("MacBuffer", allocator) MacBuffer; }
