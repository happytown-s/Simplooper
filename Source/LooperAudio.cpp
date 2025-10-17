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
	track.readPosition  = 0;
	track.recordLength  = 0;
	track.writePosition = 0;

	//TriggerEventが有効なら記録開始位置として反映
	if(triggerRef && triggerRef->triggerd)
		track.recordStartSample = static_cast<int>(triggerRef->absIndex) ;
	else
		track.recordStartSample = 0;

	DBG("🎬 Start recording track " << trackId
		<< " at sample " << track.recordStartSample);

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
		it->second.isPlaying = true;
		it->second.readPosition = 0;
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
	for (auto& [id, track] : tracks)
	{
		if (!track.isPlaying) continue;

		const int numChannels = juce::jmin(output.getNumChannels(), track.buffer.getNumChannels());
		const int numSamples = output.getNumSamples();
		const int totalSamples = track.buffer.getNumSamples();
		const int loopLength = (masterLoopLength > 0) ? masterLoopLength : juce::jmax(1, track.recordLength > 0 ? track.recordLength : track.buffer.getNumSamples());

		int remaining = numSamples; //出音側ではみ出したサンプル数
		int readPos = track.readPosition;

		while (remaining > 0)
		{
			// バッファ終端までの残りサンプル数
			int samplesToEnd = totalSamples - readPos;

			// 一度にコピーできるサンプル数（出力に残っている分 or 終端まで）
			int samplesToCopy = juce::jmin(remaining, samplesToEnd);

			// 🔊 実際にコピー
			for (int ch = 0; ch < numChannels; ++ch)
				output.addFrom(ch, numSamples - remaining, track.buffer, ch, readPos, samplesToCopy);

			// 読み取り位置と残りサンプルを更新
			readPos = (readPos + samplesToCopy) % loopLength;
			remaining -= samplesToCopy;
		}

		track.readPosition = readPos;

		// 🎛️ ログ（デバッグ用）
//		DBG("▶️ Playing track " << id
//			<< " | readPos: " << track.readPosition
//			<< " / " << track.buffer.getNumSamples());
	}
}


// 順次録音処理
void LooperAudio::startSequentialRecording(const std::vector<int>& selectedTracks)
{
	recordingQueue = selectedTracks;
	if (!recordingQueue.empty())
	{
		currentRecordingIndex = 0;
		int firstTrackId = recordingQueue[currentRecordingIndex];
		
		if (auto it = tracks.find(firstTrackId); it != tracks.end())
		{
			// 🎙️ 録音バッファ初期化
			it->second.buffer.clear();
			it->second.writePosition = 0;
			it->second.recordLength = 0;
			it->second.isPlaying = false;
			it->second.isRecording = true;

			DBG("🎬 Start sequential recording: Track " << firstTrackId);
		}

	}
	else
	{
		DBG("⚠️ No tracks selected for sequential recording!");
	}

		startRecording(recordingQueue[currentRecordingIndex]);
}


void LooperAudio::stopRecordingAndContinue()
{
	if (currentRecordingIndex >= 0 && currentRecordingIndex < (int)recordingQueue.size())
	{
		stopRecording(recordingQueue[currentRecordingIndex]);
		startPlaying(recordingQueue[currentRecordingIndex]);
		currentRecordingIndex++;
	}

	if (currentRecordingIndex < (int)recordingQueue.size())
		startRecording(recordingQueue[currentRecordingIndex]);
	else
	{
		recordingQueue.clear();
		currentRecordingIndex = -1;
	}
}

bool LooperAudio::isRecordingActive() const
{
	return (currentRecordingIndex >= 0 && currentRecordingIndex < (int)recordingQueue.size());
}

bool LooperAudio::isLastTrackRecording() const
{
	return (currentRecordingIndex >= 0 &&
			currentRecordingIndex == (int)recordingQueue.size() - 1);
}

int LooperAudio::getCurrentTrackId() const
{
	if (currentRecordingIndex >= 0 && currentRecordingIndex < (int)recordingQueue.size())
		return recordingQueue[currentRecordingIndex];
	return -1;
}

