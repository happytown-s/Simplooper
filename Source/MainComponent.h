#pragma once

#include <JuceHeader.h>
#include "LooperTrack.h"
#include "LooperAudio.h"
#include "InputTap.h"

//==============================================================================
// ルーパーアプリ本体
//==============================================================================
class MainComponent :
public juce::AudioAppComponent,
public LooperTrack::Listener,
public juce::Button::Listener,
juce::Timer
{
	public:
	MainComponent();
	~MainComponent() override;

	// AudioAppComponent
	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
	void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
	void releaseResources() override;

	// JUCE Component
	void paint(juce::Graphics&) override;
	void resized() override;

	// UIイベント
	void trackClicked(LooperTrack* track) override;
	void buttonClicked(juce::Button* button) override;
	void showDeviceSettings();

	void timerCallback()override;

	private:
	// ===== オーディオ関連 =====
	LooperAudio looper { 48000, 48000 * 10 }; // 10秒バッファ
	InputTap inputTap;

	// ===== デバイス管理 =====
	juce::AudioDeviceManager deviceManager;

	// ===== UI =====
	juce::TextButton recordButton { "Rec" };
	juce::TextButton playAllButton { "Play All" };
	juce::TextButton stopAllButton { "Stop All" };
	juce::TextButton settingButton { "Audio Settings" };

	std::vector<std::unique_ptr<LooperTrack>> tracks;
	LooperTrack* selectedTrack = nullptr;

	const int topHeight = 40;
	const int trackWidth = 100;
	const int trackHeight = 100;
	const int spacing = 10;
	const int tracksPerRow = 4;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

