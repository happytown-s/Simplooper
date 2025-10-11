/*
  ==============================================================================

    InputManager.cpp
    Created: 8 Oct 2025 3:22:59am
    Author:  mt sh

  ==============================================================================
*/

#include "InputManager.h"

void InputManager::prepare(double newSampleRate, int bufferSize)
{
	sampleRate = newSampleRate;
	triggered  = false;
	recording = false;
	triggerEvent = {};

	//あとでRingBufferの準備を追加
	//ringBuffer.prepare(numChannels, bufferSize * 8);

	DBG("InputManager::prepare sampleRate = " << sampleRate << "bufferSize = " << bufferSize);


}

void InputManager::reset()
{
	triggered = false;
	recording = false;
	triggerEvent = {};
	DBG("InputManager::reset()");
}

//==============================================================================
// メイン処理
//==============================================================================

void InputManager::analyze(const juce::AudioBuffer<float>& input)
{
	// (1) ブロック内でしきい値検知を実行
	bool trig = detectTriggerSample(input);

	// (2) 状態更新
	if (trig && !recording)
	{
		triggered = true;
		recording = true;

		DBG(" Trigger detected - Recording Start");

		//トリガー値を更新(仮、　absIndexはあとでリングバッファ実装時に設定 )
		triggerEvent.triggerd = true;
		triggerEvent.sampleInBlock = 0; //TODO 実際はdetectTriggerSample()で求めたサンプル値
		triggerEvent.absIndex = -1;
	}
	else if(!trig && recording)
	{
		//停止条件を入れる
	}
	else
	{
		triggered = false;
		triggerEvent.triggerd = false;
	}
	updateStateMachine();
}

//==============================================================================
// 閾値検知：ブロック内の最大音量を確認
//==============================================================================

bool InputManager::detectTriggerSample(const juce::AudioBuffer<float>& input)
{
	const int numChannels = input.getNumChannels();
	const int numSamples = input.getNumSamples();
	const float threshold = config.userThreshold;

	for(int s = 0; s < numSamples; ++s)
	{
		float frameAmp = 0.0f;
		for ( int ch = 0; ch < numChannels; ++ch)
		{
			frameAmp += std::abs(input.getReadPointer(ch)[s]);
		}

		frameAmp /= (float)numChannels;

		if(frameAmp > threshold)
		{
			triggerEvent.sampleInBlock = s;
			triggerEvent.channel = 0;
			return true;
		}
	}
	return false;
}

//==============================================================================
// 状態遷移（まだ仮実装）
//==============================================================================

void InputManager::updateStateMachine()
{
	// 後々「録音中→停止判定」などの処理を追加
}

//==============================================================================
// Getter / Setter
//==============================================================================

TriggerEvent InputManager::getTriggerEvent() const noexcept
{
	return triggerEvent;
}

void InputManager::setConfig(const SmartRecConfig& newConfig) noexcept
{
	config = newConfig;
}

const SmartRecConfig& InputManager::getConfig() const noexcept
{
	return config;
}
