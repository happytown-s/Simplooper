#pragma once
#include <JuceHeader.h>
#include "LooperAudio.h"
/*
  ==============================================================================

    LooperTrack.h
    Created: 21 Sep 2025 5:33:19pm
    Author:  mt sh
  ==============================================================================
*/

class LooperTrack : public juce::Component,juce::Timer

{
	public :

	enum class TrackState{
		Empty,
		Recording,
		Playing,
		Stopped,
		Idle
	};

	explicit LooperTrack(int id, TrackState initialState = TrackState::Idle)
	: trackId(id),state(initialState)
	{
		setInterceptsMouseClicks(true, true);
	}
	//リスナー関数のオーバーライド。クリックされたトラックの把握に必要
	class Listener
	{
		public:
		virtual ~Listener() = default;
		virtual void trackClicked(LooperTrack* track) = 0;
	};
	~LooperTrack() override = default;
	//トラックの状態

	void setTrackId(int id){trackId = id;}
	int getTrackId() const{return trackId;}

	void setState(TrackState newState);
	void mouseDown(const juce::MouseEvent&) override;
	void mouseEnter(const juce::MouseEvent&) override;
	void mouseExit(const juce::MouseEvent&)override;
	void setSelected(bool shouldBeSelected);
	bool getIsSelected() const;
	void setListener(Listener* listener);

	//録音処理
	void startRecording();
	void stopRecording();


	//状態管理
	TrackState getState() const{ return state;}
	juce::String getStateString() const;
	//枠の周囲の光るアニメーション用
	void startFlash();
	void timerCallback() override;
	void drawGlowingBorder(juce::Graphics& g,juce::Colour glowColour);

	protected:
	void paint(juce::Graphics& g) override;



	private:

	//マウスの状態関連
	bool isMouseOver = false;
	bool isSelected = false;
	Listener* listener = nullptr;
	//トラックの状態＆トラックNo初期化
	int trackId ;
	TrackState state;

	float flashProgress = 0.0f;
	bool isFlashing = false;
};

