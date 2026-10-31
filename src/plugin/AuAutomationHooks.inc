// Included inside JUCE 8.0.11's JuceAU class by the checked build-tree patch.
// The source checkout is untouched. See docs/HOST_AUTOMATION.md.
private:
    cs80::AuAutomationQueue cs80AuAutomation;
    struct Cs80AuCommit
    {
        JuceAU* owner = nullptr;
        AudioProcessorParameter* parameter = nullptr;
    };
    std::array<Cs80AuCommit, cs80::AuAutomationQueue::lanes> cs80AuParameters {};

    cs80::HostAutomationClient* cs80AutomationClient() const noexcept
    { return dynamic_cast<cs80::HostAutomationClient*> (juceFilter.get()); }

    int cs80AutomationIndex (AudioProcessorParameter& parameter) const noexcept
    {
        if (auto* client = cs80AutomationClient()) return client->sampleAccurateParameterIndex (parameter);
        return -1;
    }

    static void cs80CommitAuParameter (void* context, float value) noexcept
    {
        auto& commit = *static_cast<Cs80AuCommit*> (context);
        if (! approximatelyEqual (value, commit.parameter->getValue()))
        {
            commit.owner->inParameterChangedCallback = true;
            commit.parameter->setValueNotifyingHost (value);
        }
    }

    bool cs80DecodeAuEvent (const AudioUnitParameterEvent& source, AudioProcessorParameter& parameter,
                           int lane, cs80::AuAutomationQueue::Event& result) noexcept
    {
        result.lane = lane;
        const double maximum = getMaximumParameterValue (&parameter);
        if (source.eventType == kParameterEvent_Immediate)
        {
            if (source.eventValues.immediate.bufferOffset > static_cast<UInt32> (std::numeric_limits<int>::max()))
                return false;
            result.start = static_cast<int> (source.eventValues.immediate.bufferOffset);
            result.first = result.last = source.eventValues.immediate.value / maximum;
        }
        else if (source.eventType == kParameterEvent_Ramped)
        {
            if (source.eventValues.ramp.durationInFrames > static_cast<UInt32> (std::numeric_limits<int>::max()))
                return false;
            result.ramp = true;
            result.start = source.eventValues.ramp.startBufferOffset;
            result.duration = static_cast<int> (source.eventValues.ramp.durationInFrames);
            result.first = source.eventValues.ramp.startValue / maximum;
            result.last = source.eventValues.ramp.endValue / maximum;
        }
        else return false;
        return cs80AuAutomation.valid (result);
    }

    OSStatus cs80ScheduleAuImmediate (AudioProcessorParameter& parameter, float value, UInt32 offset) noexcept
    {
        AudioUnitParameterEvent source {};
        source.eventType = kParameterEvent_Immediate;
        source.eventValues.immediate.bufferOffset = offset;
        source.eventValues.immediate.value = value;
        cs80::AuAutomationQueue::Event event;
        const int lane = cs80AutomationIndex (parameter);
        if (! cs80DecodeAuEvent (source, parameter, lane, event)) return kAudioUnitErr_InvalidParameter;
        if (! cs80AuAutomation.add (event)) return kAudioUnitErr_CannotDoInCurrentContext;
        cs80AuParameters[static_cast<size_t> (lane)] = { this, &parameter };
        return noErr;
    }

    bool cs80PrepareAuAutomation (cs80::HostAutomationClient* client, int frames) noexcept
    {
        if (client == nullptr) { cs80AuAutomation.clearPending(); return true; }
        if (! client->usesHostAutomationTiming())
        {
            // AU's historical adapter applies immediate values at block start
            // and ignores scheduled ramps. Preserve that path in Pure/Legacy,
            // including a mode change after the host scheduled its events.
            cs80AuAutomation.forEachPending ([&] (const auto& event) {
                if (! event.ramp)
                    cs80CommitAuParameter (&cs80AuParameters[static_cast<size_t> (event.lane)],
                                           static_cast<float> (event.last));
            });
            cs80AuAutomation.clearPending();
        }
        std::array<double, cs80::AuAutomationQueue::lanes> initial {};
        for (size_t lane = 0; lane < initial.size(); ++lane)
            initial[lane] = cs80AuParameters[lane].parameter != nullptr
                ? cs80AuParameters[lane].parameter->getValue() : client->sampleAccurateParameterValue (lane);
        if (! cs80AuAutomation.prepare (initial, frames)) return false;
        for (size_t lane = 0; lane < initial.size(); ++lane)
        {
            auto queue = cs80AuAutomation.queue (lane);
            if (queue.count == 0) continue;
            queue.parameter = &cs80AuParameters[lane];
            queue.commitParameter = cs80CommitAuParameter;
            if (! client->attachHostAutomationQueue (*cs80AuParameters[lane].parameter, queue)) return false;
        }
        return true;
    }

public:
    OSStatus ScheduleParameter (const AudioUnitParameterEvent* events, UInt32 count) override
    {
        if (events == nullptr && count != 0) return kAudioUnitErr_InvalidParameter;
        size_t scheduled = 0;
        // Validate the whole batch and capacity before accepting its first item.
        for (UInt32 i = 0; i < count; ++i)
        {
            const auto& source = events[i];
            if (source.scope != kAudioUnitScope_Global || source.element != 0) return kAudioUnitErr_InvalidScope;
            auto* parameter = getParameterForAUParameterID (source.parameter);
            if (parameter == nullptr) return kAudioUnitErr_InvalidParameter;
            const int lane = cs80AutomationIndex (*parameter);
            if (lane >= 0)
            {
                cs80::AuAutomationQueue::Event event;
                if (! cs80DecodeAuEvent (source, *parameter, lane, event)) return kAudioUnitErr_InvalidParameter;
                ++scheduled;
            }
            else if (source.eventType != kParameterEvent_Immediate && source.eventType != kParameterEvent_Ramped)
                return kAudioUnitErr_InvalidParameter;
        }
        if (scheduled > cs80AuAutomation.available()) return kAudioUnitErr_CannotDoInCurrentContext;
        for (UInt32 i = 0; i < count; ++i)
        {
            const auto& source = events[i];
            auto* parameter = getParameterForAUParameterID (source.parameter);
            const int lane = cs80AutomationIndex (*parameter);
            if (lane >= 0)
            {
                cs80::AuAutomationQueue::Event event;
                const bool valid = cs80DecodeAuEvent (source, *parameter, lane, event);
                jassert (valid);
                if (! valid || ! cs80AuAutomation.add (event)) return kAudioUnitErr_CannotDoInCurrentContext;
                cs80AuParameters[static_cast<size_t> (lane)] = { this, parameter };
            }
            else if (source.eventType == kParameterEvent_Immediate)
            {
                const auto result = SetParameter (source.parameter, source.scope, source.element,
                    source.eventValues.immediate.value, source.eventValues.immediate.bufferOffset);
                if (result != noErr) return result;
            }
        }
        return noErr;
    }
