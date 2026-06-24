#include <PortAudio/include/portaudio.h>
#define HAVE_C99_VARARGS_MACROS
#include <aubio/src/aubio.h>
#include <firesteel/utils/log.hpp>

const int SAMPLE_RATE = 44100;
const int HOP_SIZE = 512;
const int BUFFER_SIZE = 2048;

namespace AudioIO {
	struct AudioData {
		aubio_pitch_t* pitch_processor;
		fvec_t* in;
		fvec_t* out;
	};
	struct AudioReciever {
		AudioData data;
		PaStream* stream;
		bool streamOpen;
	};
	AudioReciever reciever;

	static int streamCallback(const void* inBuf, void* outBuf, ulong framesPerBuf, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags flags, void* userData) {
		auto* data = static_cast<AudioData*>(userData);
		const float* in = static_cast<const float*>(inBuf);
		if (inBuf == nullptr) return paContinue;
		for (uint i = 0;i < framesPerBuf;i++) {
			data->in->data[i] = in[i];
		}
		smpl_t rms = aubio_silence_detection(data->in, -90);
		aubio_pitch_do(data->pitch_processor, data->in, data->out);
		float pitchHz = data->out->data[0];
		if (rms < -45.f) {
			LOGF("Volume: %s | Pitch: %s", rms, pitchHz);
		} else {
			LOGF("No input");
		}
		return paContinue;
	}

	bool initialize() {
		reciever.streamOpen = false;
		reciever.data.pitch_processor = new_aubio_pitch("yinfft", BUFFER_SIZE, HOP_SIZE, SAMPLE_RATE);
		reciever.data.in = new_fvec(HOP_SIZE);
		reciever.data.out = new_fvec(1);
		aubio_pitch_set_unit(reciever.data.pitch_processor, "Hz");
		if (Pa_Initialize() != paNoError) {
			LOG_ERRR("Failed to initialize PortAudio");
			return false;
		}

		PaStreamParameters streamParams{};
		streamParams.device = Pa_GetDefaultInputDevice();
		if (streamParams.device == paNoDevice) {
			LOG_ERRR("Failed to detect audio input device");
			return false;
		}
		streamParams.channelCount = 1;
		streamParams.sampleFormat = paFloat32;
		streamParams.suggestedLatency = Pa_GetDeviceInfo(streamParams.device)->defaultLowInputLatency;
		streamParams.hostApiSpecificStreamInfo = nullptr;

		if (Pa_OpenStream(&reciever.stream, &streamParams, nullptr, SAMPLE_RATE, HOP_SIZE, paClipOff, nullptr/*callback*/, &reciever.data) != paNoError) {
			LOG_ERRR("Failed to open input stream");
			return false;
		}
		reciever.streamOpen = true;

		return true;
	}

	void start() {
		Pa_StartStream(reciever.stream);
	}

	void stop() {
		Pa_StopStream(reciever.stream);
	}

	void remove() {
		reciever.streamOpen = false;
		Pa_CloseStream(reciever.stream);
		reciever.stream = nullptr;
		Pa_Terminate();
		del_aubio_pitch(reciever.data.pitch_processor);
		del_fvec(reciever.data.in);
		del_fvec(reciever.data.out);
	}
}