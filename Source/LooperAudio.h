#pragma once
#include <JuceHeader.h>
#include "TriggerEvent.h"
#include <map>



class LooperAudio
{
	public:

	LooperAudio(TriggerEvent& sharedTrigger,double sampleRate = 44100.0,int maxSamples = 480000);

	~LooperAudio() = default;

	void prepareToPlay(int samplesPerBlockExpected, double sampleRate);
	void processBlock(juce::AudioBuffer<float>& output, const juce::AudioBuffer<float>& input);

	void addTrack(int trackId);
	void startRecording(int trackId);
	void stopRecording(int trackId);
	void startPlaying(int trackId);
	void stopPlaying(int trackId);
	void clearTrack(int trackId);

	void startSequentialRecording(const std::vector<int>& selectedTracks);
	void stopRecordingAndContinue();
	bool isRecordingActive() const;
	bool isLastTrackRecording() const;
	int getCurrentTrackId() const;

	const TriggerEvent& getTriggerRef() const noexcept {return triggerRef;}


	

private:
	struct TrackData
	{
		juce::AudioBuffer<float> buffer;
		bool isRecording = false;
		bool isPlaying = false;
		int writePosition = 0;
		int readPosition = 0;
		int recordLength = 0;
		int recordStartSample = 0; //グローバル位置での録音開始サンプル
		int lengthInSample = 0; //トラックの長さ

	};

	std::map<int, TrackData> tracks;

	TriggerEvent& triggerRef;
	double sampleRate;
	int maxSamples;

	int masterTrackId = -1;
	int masterLoopLength = 0;
	long currentSamplePosition = 0;

	std::vector<int> recordingQueue;
	int currentRecordingIndex = -1;

	TriggerEvent lastTriggerEvent;
	//マスターの録音開始位置
	int masterStartSample    = 0;


	void recordIntoTracks(const juce::AudioBuffer<float>& input);
	void mixTracksToOutput(juce::AudioBuffer<float>& output);
};

