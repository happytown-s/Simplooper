/*
  ==============================================================================

    LooperTrack.cpp
    Created: 21 Sep 2025 5:33:29pm
    Author:  mt sh

  ==============================================================================
*/
#include "LooperTrack.h"


void LooperTrack::paint(juce::Graphics& g)
{

	auto bounds = getLocalBounds().toFloat();

	if(isSelected){
		g.setColour(juce::Colours::darkorange);
	}else if(isMouseOver){
		g.setColour(juce::Colours::skyblue);
	}else{
		g.setColour(juce::Colours::darkgrey);
	}
	g.drawRoundedRectangle(bounds, 10.0f, 4.0f);

	if(state == TrackState::Recording){
		g.setColour(juce::Colours::darkred);
		g.drawRoundedRectangle(bounds, 10.0f, 4.0f);
		drawGlowingBorder(g, juce::Colours::red);

	}else if(state == TrackState::Playing){
		g.setColour(juce::Colours::darkgreen);
		g.drawRoundedRectangle(bounds, 10.0f, 4.0f);
		drawGlowingBorder(g, juce::Colours::green);
	}

}

void LooperTrack::drawGlowingBorder(juce::Graphics& g,juce::Colour glowColour){
	auto bounds = getLocalBounds().toFloat();

	float totalPerimeter = bounds.getWidth() * 2 + bounds.getHeight()* 2;
	float drawLength = flashProgress * totalPerimeter;

	juce::Line<float> lines[4] = {
		{bounds.getTopLeft(),bounds.getTopRight()},
		{bounds.getTopRight(),bounds.getBottomRight()},
		{bounds.getBottomRight(),bounds.getBottomLeft()},
		{bounds.getBottomLeft(),bounds.getTopLeft()}
	};
	g.setColour(glowColour);

	float remaining = drawLength;

	for(int i = 0; i < 4; ++i){
		auto lineLength = lines[i].getLength();
		if(remaining <= 0) break;

		if(remaining < lineLength){

			//線の一部だけ描く
			juce::Point<float> end = lines[i].getStart() + (lines[i].getEnd() - lines[i].getStart()) * (remaining / lineLength);
			g.drawLine(lines[i].getStart().x,lines[i].getStart().y,end.x,end.y,3.0f);
			break;
		}
		else{
			//線全体を描く
			g.drawLine(lines[i], 3.0f);
			remaining -= lineLength;
		}
	}
}


void LooperTrack::mouseDown(const juce::MouseEvent&)
{
	if(listener != nullptr){
		listener->trackClicked(this);
	}
	//juce::Logger::writeToLog("Track Clicked");
}
void LooperTrack::mouseEnter(const juce::MouseEvent&){
	isMouseOver = true;
	//DBG("MouseOver");
	repaint();
}
void LooperTrack::mouseExit(const juce::MouseEvent&){
	isMouseOver = false;
	//juce::Logger::writeToLog("MouseExit");
	repaint();
}
void LooperTrack::setSelected(bool shouldBeSelected){
	isSelected = shouldBeSelected;
	repaint();
}
bool LooperTrack::getIsSelected() const {return isSelected;}

void LooperTrack::setListener(Listener* listener) { this->listener = listener;}


void LooperTrack::setState(TrackState newState)
{
	state = newState;

	switch (state)
	{
		case TrackState::Empty:
			break;
		case TrackState::Recording:
			setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
			break;
		case TrackState::Playing:
			setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);
			break;
		case TrackState::Stopped:
			setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
			break;
		case TrackState::Idle:
			setColour(juce::TextButton::buttonColourId, juce::Colours::grey);
			break;

//			Empty,
//			Recording,
//			Playing,
//			Stopped,
//			Idle
	}
	repaint();
}
juce::String LooperTrack::getStateString()const {
	switch (state) {
		case TrackState::Empty: return "Empty";
		case TrackState::Recording: return "Recording";
		case TrackState::Playing: return "Playing";
		case TrackState::Stopped: return "Stopped";
		case TrackState::Idle: return "Idle";
	}
	return "Unknown";
}


void LooperTrack::startRecording(){
	setState(TrackState::Recording);
	flashProgress = 0.0f;
	startTimerHz(60); //60FPS
}
void LooperTrack::stopRecording(){
	setState(TrackState::Playing);
	flashProgress = 0.0f;
	startTimerHz(60);
}

void LooperTrack::startFlash(){
	isFlashing = true;
	flashProgress = 0.0f;
}



void LooperTrack::timerCallback(){
	if(state == TrackState::Recording || state == TrackState::Playing){
		flashProgress += 0.02f; //1周で50フレーム
		if(flashProgress >= 1.0f){
			flashProgress = 0.0f;
		}
		repaint();
	}else{
		stopTimer();
	}
}

