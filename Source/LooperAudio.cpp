#include "LooperAudio.h"

LooperAudio::LooperAudio(double sr, int max)
: sampleRate(sr), maxSamples(max)
{
}

void LooperAudio::prepareToPlay(int samplesPerBlockExpected, double sr)
{
	sampleRate = sr;
}

void LooperAudio::processBlock(juce::AudioBuffer<float>& output,
							   const juce::AudioBuffer<float>& input)
{
//	if (input.getNumChannels() > 0)
//	{
//		auto range = juce::FloatVectorOperations::findMinAndMax(input.getReadPointer(0),
//																input.getNumSamples());
//		//if (std::abs(range.getStart()) > 0.001f || std::abs(range.getEnd()) > 0.001f)
//		//	DBG("🎙️ Mic Level: " << range.getStart() << " ~ " << range.getEnd());
//	}

	// 録音・再生処理
	recordIntoTracks(input);
	output.clear();
	mixTracksToOutput(output);

	const int numChannels = juce::jmin(input.getNumChannels(), output.getNumChannels());
	const int numSamples = input.getNumSamples();

	for (int ch =0; ch < numChannels; ++ch)
	{
		output.addFrom(ch, 0, input, ch, 0, numSamples);
	}
	

//	DBG("Output level: " << output.getRMSLevel(0, 0, output.getNumSamples()));
	

	
//	float rms = output.getRMSLevel(0, 0, output.getNumSamples());
//	if (rms > 0.001f)
//		DBG("🔊 Output RMS: " << rms);
}


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
	if (auto it = tracks.find(trackId); it != tracks.end())
	{
		it->second.isRecording = true;
		it->second.writePosition = 0;
	}
}

void LooperAudio::stopRecording(int trackId)
{
	if (auto it = tracks.find(trackId); it != tracks.end())
	{
		it->second.isRecording = false;
		it->second.recordLength = it->second.writePosition;
	}
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
		const int numSamples = input.getNumSamples();

		int samplesAvailable = track.buffer.getNumSamples() - track.writePosition;
		int samplesToCopy = juce::jmin(input.getNumSamples(), samplesAvailable);

		for (int ch = 0; ch < numChannels; ++ch)
		{
			track.buffer.copyFrom(ch, track.writePosition, input, ch, 0, samplesToCopy);
			DBG("Recording samples to Track " << id << " pos: " << track.writePosition);
		}

		track.writePosition += samplesToCopy;

		if (track.writePosition >= track.buffer.getNumSamples())
			track.writePosition = 0;
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
		const int loopLength = juce::jmax(1, track.recordLength);

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
		DBG("▶️ Playing track " << id
			<< " | readPos: " << track.readPosition
			<< " / " << track.buffer.getNumSamples());
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

