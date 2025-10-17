#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
	: sharedTrigger(inputTap.getTriggerEvent()),
	looper(sharedTrigger)
{
	setAudioChannels(2, 2);
	deviceManager.addAudioCallback(&inputTap); // 入力だけTapする

	startTimerHz(30);

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

	DBG("InputTap trigger address = " + juce::String((juce::uint64)(uintptr_t)&inputTap.getTriggerEvent()));
	DBG("Shared trigger address   = " + juce::String((juce::uint64)(uintptr_t)&sharedTrigger));
	DBG("Looper trigger address   = " + juce::String((juce::uint64)(uintptr_t)&looper.getTriggerRef()));

}

void MainComponent::releaseResources()
{
	DBG("releaseResources called");
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
	auto& trig = sharedTrigger;
	bufferToFill.clearActiveBufferRegion();

	// 入力バッファを取得
	juce::AudioBuffer<float> input(bufferToFill.buffer->getNumChannels(),
								   bufferToFill.numSamples);
	input.clear();
	inputTap.getLatestInput(input);

	// === トリガーが立ったら ===
	if (trig.triggerd)
	{


		// UIスレッドで安全に録音処理 & 見た目更新
		juce::MessageManager::callAsync([this, trig]()
		{
			bool anyRecording = false;
			for (auto& t : tracks)
			{
				if (t->getState() == LooperTrack::TrackState::Recording)
				{
					anyRecording = true;
					break;
				}
			}

			if (!anyRecording)
			{
				std::vector<int> selectedIDs;
				for (auto& t : tracks)
				{
					if (t->getIsSelected())
					{
						selectedIDs.push_back(t->getTrackId());
						t->setState(LooperTrack::TrackState::Recording);
						t->repaint(); // 🎨 見た目更新
						DBG("🎚 Auto trigger selected track: " << t->getTrackId());
					}
				}

				if (!selectedIDs.empty())
				{
					for (int id : selectedIDs)
						looper.startRecording(id);

					// ✅ ボタンのUI更新
					recordButton.setButtonText("Recording...");
					recordButton.setColour(juce::TextButton::buttonColourId,
										   juce::Colours::darkred);

					DBG("🎬 Auto-triggered recording started!");
				}
				else
				{
					DBG("⚠️ Trigger detected but no track selected!");
				}
			}
			else
			{
				recordButton.setButtonText("Rec");
				recordButton.setColour(juce::TextButton::buttonColourId,
									   juce::Colours::darkgrey);
			}

		});

		DBG("triggerd is " << (trig.triggerd ? "true" : "false"));
		// リセット（Audioスレッド側）
		trig.triggerd = false;
		trig.sampleInBlock = -1;
		trig.absIndex = -1;
		DBG("triggerd is " << (trig.triggerd ? "true" : "false"));


	}
	

	// 🌀 LooperAudio の処理は常に実行
	looper.processBlock(*bufferToFill.buffer, input);
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
		std::vector<int> selectedIDs;

		//選択されたトラックを集める
		for (auto& t : tracks)
		{
			if (t->getIsSelected())
			{
				selectedIDs.push_back(t->getTrackId());
			}
		}

		if (selectedIDs.empty())
		{
			DBG("⚠️ No track selected for recording!");
		}

		bool anyRecording = false;
		// 録音中 → 録音停止＆再生開始
		for (auto& t : tracks)
		{
			if(t->getState() == LooperTrack::TrackState::Recording)
			{
				anyRecording = true;
				break;
			}
		}

		if(anyRecording)
		{
			// 🎛 全録音停止 → 再生へ
			for (auto& t : tracks)
			{
				if (t->getState() == LooperTrack::TrackState::Recording)
				{
					int id = t->getTrackId();
					looper.stopRecording(id);
					looper.startPlaying(id);
					t->setState(LooperTrack::TrackState::Playing);
					t->setSelected(false);
					//DBG("selected = " << (t->getIsSelected() ? "true" : "false"));
				}
			}
			// 🎯 トリガーのリセット（誤作動防止）
			auto& trig = inputTap.getManager().getTriggerEvent();
			trig.triggerd = false;
			trig.sampleInBlock = -1;
			trig.absIndex = -1;
			DBG("🎛 Trigger reset after stop");
		}
		else
		{
			// 🎬 録音開始
			for (auto& t : tracks)
			{
				if (t->getIsSelected())
				{
					int id = t->getTrackId();
					looper.startRecording(id);
					t->setState(LooperTrack::TrackState::Recording);
					DBG("🎙 Start recording track " << id);
				}
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
	else if (button == &playAllButton)
	{
		for (auto& t : tracks)
		{
			int id = t->getTrackId();

			if(t->getState() == LooperTrack::TrackState::Idle)
				continue;
			looper.startPlaying(id);
			t->setState(LooperTrack::TrackState::Playing);
			t->repaint();
		}
		recordButton.setButtonText("Play");
		recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);
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

void MainComponent::updateStateVisual()
{
	bool anyRecording = false;
	bool anyPlaying = false;
	bool selectedDuringPlay = false;

	for(auto& t : tracks)
	{
		switch (t->getState()) {
			case LooperTrack::TrackState::Recording:
				anyRecording = true;
				break;
			case LooperTrack::TrackState::Playing:
				anyPlaying = true;
				break;
			default:
				break;
		}
		if(t->getIsSelected() && anyPlaying)
			selectedDuringPlay = true;
	}

	if(anyRecording)
	{
		recordButton.setButtonText("Play");
		recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);
	}
	else if(anyPlaying && selectedDuringPlay)
	{recordButton.setButtonText("Next");
		recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkorange);
	}else if(anyPlaying)
	{recordButton.setButtonText("Stop");
		recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
	}else
	{
		recordButton.setButtonText("Record");
		recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
	}

	//DBG("UpdateVisual!!");
	repaint();
}

void MainComponent::startRec()
{

		std::vector<int> selectedIDs;

		//現在の選択トラックのIDを収集
		for (auto& t : tracks)
		{
			if(t->getIsSelected())
			{
				int id = t->getTrackId();
				selectedIDs.push_back(id);
				DBG("🎚 Selected for trigger: Track " << id);
			}

			if(!selectedIDs.empty())
			{
				DBG("🎬 Auto recording triggered (direct startRecording)");

				for (int id : selectedIDs)
				{
					looper.startRecording(id);
				}

			}
			else
			{
				DBG("⚠️ Trigger detected but no track selected!");


			}
		}
}

void MainComponent::timerCallback()
{
	if(inputTap.triggerFlag.exchange(false))
		DBG("TriggerDetected!");

	for (auto& t :tracks)
	{
		updateStateVisual();
	}
}
