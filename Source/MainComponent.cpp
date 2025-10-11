#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
	setAudioChannels(2, 2);
	deviceManager.addAudioCallback(&inputTap); // 入力だけTapする


	// トラック初期化（最初は4つ）
	for (int i = 0; i < 4; ++i)
	{
		int newId = static_cast<int>(tracks.size() + 1);
		auto track = std::make_unique<LooperTrack>(newId, LooperTrack::TrackState::Idle);
		track->setListener(this);
		addAndMakeVisible(track.get());
		tracks.push_back(std::move(track));
		looper.addTrack(newId);
	}

	// ボタン類設定
	addAndMakeVisible(recordButton);
	addAndMakeVisible(playAllButton);
	addAndMakeVisible(stopAllButton);
	addAndMakeVisible(settingButton);

	recordButton.addListener(this);
	playAllButton.addListener(this);
	stopAllButton.addListener(this);
	settingButton.onClick = [this] { showDeviceSettings(); };

	recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
	playAllButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);
	stopAllButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);

	setSize(720, 600);
}

MainComponent::~MainComponent()
{
	shutdownAudio();
}

//==============================================================================

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
	inputTap.prepare(sampleRate, samplesPerBlockExpected);
	looper.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::releaseResources()
{
	DBG("releaseResources called");
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
	auto trig = inputTap.getManager().getTriggerEvent();
	// 出力バッファをクリア
	bufferToFill.clearActiveBufferRegion();

	// 最新の入力を取得
	juce::AudioBuffer<float> input(bufferToFill.buffer->getNumChannels(),
								   bufferToFill.numSamples);
	input.clear();
	inputTap.getLatestInput(input);

	//入力を検知
//	if(trig.triggerd)
//	{
//		DBG("🎬 Auto recording triggered!");
//		looper.startSequentialRecording({1});
//	}
	// 入力 → LooperAudioへ
	looper.processBlock(*bufferToFill.buffer, input);



//	DBG("Main Out RMS: " << bufferToFill.buffer->getRMSLevel(0, 0, bufferToFill.numSamples));

}



//==============================================================================

void MainComponent::paint(juce::Graphics& g)
{
	g.fillAll(juce::Colours::black);
}

void MainComponent::resized()
{
	auto area = getLocalBounds().reduced(15);
	auto topArea = area.removeFromTop(topHeight);

	recordButton.setBounds(topArea.removeFromLeft(100).reduced(5));
	playAllButton.setBounds(topArea.removeFromLeft(100).reduced(5));
	stopAllButton.setBounds(topArea.removeFromLeft(100).reduced(5));
	settingButton.setBounds(topArea.removeFromLeft(150).reduced(5));

	int x = 0, y = 0;
	for (int i = 0; i < tracks.size(); i++)
	{
		int row = i / tracksPerRow;
		int col = i % tracksPerRow;
		x = col * (trackWidth + spacing);
		y = row * (trackHeight + spacing);
		tracks[i]->setBounds(area.getX() + x + spacing,
							 area.getY() + y + spacing,
							 trackWidth, trackHeight);
	}
}

//==============================================================================

void MainComponent::trackClicked(LooperTrack* track)
{
	DBG("Clicked track ID: " + juce::String(track->getTrackId()));

	for (auto& t : tracks)
		t->setSelected(t.get() == track ? !t->getIsSelected() : t->getIsSelected());
}

void MainComponent::buttonClicked(juce::Button* button)
{
	if (button == &recordButton)
	{
		if (!looper.isRecordingActive())
		{
			std::vector<int> selectedIDs;
			for (auto& t : tracks)
			{
				if (t->getIsSelected())
				{
					int id = t->getTrackId();
					t->setState(LooperTrack::TrackState::Recording);
					selectedIDs.push_back(id);
				}
			}

			if (!selectedIDs.empty())
			{
				looper.startSequentialRecording(selectedIDs);
				recordButton.setButtonText("Next..");
				recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
			}
		}
		else
		{
			looper.stopRecordingAndContinue();

			if (looper.isRecordingActive())
				recordButton.setButtonText("Next..");
			else
			{
				recordButton.setButtonText("Rec");
				recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
			}
		}
	}
	else if (button == &stopAllButton)
	{
		for (auto& t : tracks)
		{
			int id = t->getTrackId();
			looper.stopRecording(id);
			looper.stopPlaying(id);
			t->setState(LooperTrack::TrackState::Stopped);
		}
	}
}

//==============================================================================

void MainComponent::showDeviceSettings()
{
	auto* selector = new juce::AudioDeviceSelectorComponent(
															deviceManager,
															0, 2,   // min/max input
															0, 2,   // min/max output
															false, false,
															true, true
															);
	selector->setSize(520, 360);

	juce::DialogWindow::LaunchOptions opts;
	opts.dialogTitle = "Audio Device Settings";
	opts.content.setOwned(selector);
	opts.componentToCentreAround = this;
	opts.useNativeTitleBar = true;
	opts.escapeKeyTriggersCloseButton = true;
	opts.launchAsync();
}


void MainComponent::timerCallback()
{
	if(inputTap.triggerFlag.exchange(false))
		DBG("TriggerDetected!");
}
