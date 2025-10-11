#pragma once
#include <JuceHeader.h>
#include <map>

struct TrackData
{
	juce::AudioBuffer<float> buffer;
	bool isRecording = false;
	bool isPlaying = false;
	int writePosition = 0;
	int readPosition = 0;

	int recordLength = 0;
};

class LooperAudio
{
	public:
	LooperAudio(double sampleRate, int maxSamples);
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

	private:
	std::map<int, TrackData> tracks;
	double sampleRate;
	int maxSamples;

	std::vector<int> recordingQueue;
	int currentRecordingIndex = -1;

	void recordIntoTracks(const juce::AudioBuffer<float>& input);
	void mixTracksToOutput(juce::AudioBuffer<float>& output);
};

