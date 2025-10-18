#include "LooperAudio.h"


LooperAudio::LooperAudio(double sr, int max)
: sampleRate(sr), maxSamples(max)
{
}

LooperAudio::~LooperAudio()
{
	//removeListener(listeners);
}

void LooperAudio::prepareToPlay(int samplesPerBlockExpected, double sr)
{
	sampleRate = sr;
}

void LooperAudio::processBlock(juce::AudioBuffer<float>& output,
							   const juce::AudioBuffer<float>& input)
{
	// 録音・再生処理
	output.clear();
	recordIntoTracks(input);
	mixTracksToOutput(output);

	//入力音をモニター出力
	const int numChannels = juce::jmin(input.getNumChannels(), output.getNumChannels());
	const int numSamples = input.getNumSamples();

	for (int ch =0; ch < numChannels; ++ch)
	{
		output.addFrom(ch, 0, input, ch, 0, numSamples);
	}

//	float rms = output.getRMSLevel(0, 0, output.getNumSamples());
//	if (rms > 0.001f)
//		DBG("🔊 Output RMS: " << rms);
}

//------------------------------------------------------------
// トラック管理
void LooperAudio::addTrack(int trackId)
{
	TrackData track;
	track.buffer.setSize(2, maxSamples);
	track.buffer.clear();
	tracks[trackId] = std::move(track);
}

void LooperAudio::startRecording(int trackId)
{
	auto& track = tracks[trackId];
	track.isRecording = true;
	track.isPlaying     = false;
	track.recordLength  = 0;

	//マスターが再生中なら、その位置から録音開始
	if (masterLoopLength > 0 && tracks.find(masterTrackId) != tracks.end() &&tracks[masterTrackId].isPlaying)
	{
		//マスターの位置に同期させる
		track.writePosition = masterReadPosition;
		track.recordStartSample = masterReadPosition;
		DBG("🎬 Start recording track " << trackId
			<< " aligned with master at position " << masterReadPosition);

	}//TriggerEventが有効なら記録開始位置として反映
	else if(triggerRef && triggerRef->triggerd)
	{
		track.recordStartSample = static_cast<int>(triggerRef->absIndex) ;
		track.writePosition = juce::jlimit(0, maxSamples -1, (int)triggerRef->absIndex);
		DBG("🎬 Start recording track " << trackId
			<< " triggered at " << triggerRef->absIndex);
	}else
	{
		track.readPosition  = 0;
		track.writePosition= 0;
		track.recordStartSample = 0;

		DBG("🎬 Start recording track " << trackId << " from beginning");
	}
	track.buffer.clear();

	listeners.call([&] (Listener& l) { l.onRecordingStarted(trackId); });
}
//------------------------------------------------------------


void LooperAudio::stopRecording(int trackId)
{
	auto& track = tracks[trackId];
	track.isRecording = false;

	// 現在の録音長を保持
	const int recordedLength = track.writePosition;
	if (recordedLength <= 0) return;

	if (masterLoopLength <= 0)
	{
		// 録音長をそのままマスター長に採用
		masterTrackId = trackId;
		masterLoopLength = recordedLength;
		track.recordLength = masterLoopLength;
		track.lengthInSample = masterLoopLength;
		masterStartSample = track.recordStartSample;

		DBG("🎛 Master loop length set to " << masterLoopLength
			<< " samples | recorded=" << recordedLength
			<< " | masterStart=" << masterStartSample);
		//return;
	}else
	{

		juce::AudioBuffer<float> aligned;
		aligned.setSize(2, masterLoopLength,false,false,true);
		aligned.clear();

		const int copyLen = juce::jmin(recordedLength, masterLoopLength);

		aligned.copyFrom(0, 0, track.buffer, 0, 0, copyLen);
		aligned.copyFrom(1, 0, track.buffer, 1, 0, copyLen);


		// 🎯 整列済みループを保存
		track.buffer.makeCopyOf(aligned);
		track.lengthInSample = masterLoopLength;
		track.recordLength = copyLen;

		DBG("🟢 Track " << trackId << ": aligned to master (length " << masterLoopLength << ")");
	}

	listeners.call([&] (Listener& l){ l.onRecordingStopped(trackId); });
}

void LooperAudio::startPlaying(int trackId)
{
	if (auto it = tracks.find(trackId); it != tracks.end())
	{
		auto& track = it->second;
		track.isPlaying = true;

		// 🔥 再生開始位置をマスター位置に合わせる
		if (masterLoopLength > 0)
		{
			track.readPosition = masterReadPosition % masterLoopLength;
		}
		else
		{
			track.readPosition = 0;
		}

		DBG("▶️ Start playing track " << trackId
			<< " aligned to master at " << track.readPosition);
	}
}


void LooperAudio::stopPlaying(int trackId)
{
	if (auto it = tracks.find(trackId); it != tracks.end())
		it->second.isPlaying = false;
}

void LooperAudio::clearTrack(int trackId)
{
	if (auto it = tracks.find(trackId); it != tracks.end())
		it->second.buffer.clear();
}


// 録音・再生処理


void LooperAudio::recordIntoTracks(const juce::AudioBuffer<float>& input)
{

	for (auto& [id, track] : tracks)
	{
		if (!track.isRecording) continue;


		const int numChannels = juce::jmin(input.getNumChannels(), track.buffer.getNumChannels());
		const int numSamples  = input.getNumSamples();

		const int loopLength = (masterLoopLength > 0) ? masterLoopLength : track.buffer.getNumSamples();

		int remaining = loopLength - track.writePosition;

		int samplesToCopy = juce::jmin(numSamples, remaining);

		for(int ch = 0; ch < numChannels; ++ch)
			track.buffer.copyFrom(ch, track.writePosition, input, ch, 0, samplesToCopy);

		// 🧮 書き込み位置をループに沿って進める

		track.writePosition += samplesToCopy;

		// 🔚 もし録音が1周分終わったら止める
		track.recordLength += numSamples;
		if (track.recordLength >= loopLength)
		{

			stopRecording(id);


			startPlaying(id);

			DBG("✅ Track " << id << " finished seamless loop (" << loopLength << " samples)");

		}

	}

}
void LooperAudio::mixTracksToOutput(juce::AudioBuffer<float>& output)
{
	const int numSamples = output.getNumSamples();

	for (auto& [id, track] : tracks)
	{
		if (!track.isPlaying) continue;

		const int numChannels = juce::jmin(output.getNumChannels(), track.buffer.getNumChannels());
		const int totalSamples = track.buffer.getNumSamples();
		const int loopLength = (masterLoopLength > 0)
		? masterLoopLength
		: juce::jmax(1, track.recordLength > 0 ? track.recordLength : track.buffer.getNumSamples());

		int remaining = numSamples;
		int readPos = track.readPosition;

		while (remaining > 0)
		{
			int samplesToEnd = totalSamples - readPos;
			int samplesToCopy = juce::jmin(remaining, samplesToEnd);

			for (int ch = 0; ch < numChannels; ++ch)
				output.addFrom(ch, numSamples - remaining, track.buffer, ch, readPos, samplesToCopy);

			readPos = (readPos + samplesToCopy) % loopLength;
			remaining -= samplesToCopy;
		}

		track.readPosition = readPos;
	}

	// ✅ ここでマスターを独立して進める
	if (masterLoopLength > 0)
	{
		masterReadPosition = (masterReadPosition + numSamples) % masterLoopLength;
	}
}
