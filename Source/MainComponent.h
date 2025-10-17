#pragma once

#include <JuceHeader.h>
#include "LooperTrackUi.h"
#include "LooperAudio.h"
#include "InputTap.h"
#include "Util.h"

//==============================================================================
// ルーパーアプリ本体
//==============================================================================
class MainComponent :
public juce::AudioAppComponent,
public LooperTrackUi::Listener,
public juce::Button::Listener,
public LooperAudio::Listener,
juce::Timer
{
	public:
	MainComponent();
	void onRecordingStarted(int trackID) override;
	void onRecordingStopped(int trackID) override;


	~MainComponent() override;

	// AudioAppComponent
	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
	void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
	void releaseResources() override;

	// JUCE Component
	void paint(juce::Graphics&) override;
	void resized() override;

	// UIイベント
	void trackClicked(LooperTrackUi* trackClicked) override;
	void buttonClicked(juce::Button* button) override;
	void showDeviceSettings();
	void updateStateVisual();

	void startRec();



	private:
	// ===== オーディオ関連 =====
	InputTap inputTap;
	juce::TriggerEvent& sharedTrigger;
	LooperAudio looper ;//10秒バッファ

	void timerCallback()override;



	// ===== デバイス管理 =====
	juce::AudioDeviceManager deviceManager;

	// ===== UI =====
	juce::TextButton recordButton { "Rec" };
	juce::TextButton playAllButton { "Play All" };
	juce::TextButton stopAllButton { "Stop All" };
	juce::TextButton settingButton { "Audio Settings" };

	std::vector<std::unique_ptr<LooperTrackUi>> tracks;
	LooperTrackUi* selectedTrack = nullptr;

	const int topHeight = 40;
	const int trackWidth = 100;
	const int trackHeight = 100;
	const int spacing = 10;
	const int tracksPerRow = 4;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

