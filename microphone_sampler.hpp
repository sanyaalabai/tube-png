#include <PortAudio/include/portaudio.h>
#define HAVE_C99_VARARGS_MACROS
#include <aubio/src/aubio.h>
#include <firesteel/util/log.hpp>

const int SAMPLE_RATE = 44100;
const int HOP_SIZE = 512;
const int BUFFER_SIZE = 2048;

namespace AudioIO {
	struct AudioData {
		aubio_pitch_t* pitchProcessor=nullptr;
		fvec_t* in=nullptr;
		fvec_t* out=nullptr;
		float rms=0;
		float dB=0;
		float pitch=0;
	};
	struct AudioReciever {
		const char* name="";
		AudioData data{};
		PaStream* stream=nullptr;
		bool streamOpen=false;
		bool muted=false;

		float amplifier=1.f;
		float cutOff=-45.f;
	};
	AudioReciever reciever;

	std::vector<const PaDeviceInfo*> getInputDevices() {
		std::vector<const PaDeviceInfo*> o;
		int numDevices = Pa_GetDeviceCount();
		if(numDevices<0) {
			LOGF_ERRR("PortAudio error getting device count: %i", numDevices);
			return o;
		}

		for(int i=0;i<numDevices;i++) {
			const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
			if(deviceInfo->maxInputChannels>0) {
#ifdef _WIN32
				std::string name(deviceInfo->name);
				//Remove fake Windows input devices.
				if(name.find("[Loopback]")!=std::string::npos ||
					name.find("loopback")!=std::string::npos) continue;
				const PaHostApiInfo* hostInfo=Pa_GetHostApiInfo(deviceInfo->hostApi);
				if(hostInfo && hostInfo->type!=paWASAPI) continue;
#endif // _WIN32
				o.push_back(deviceInfo);
			}
		}
		return o;
	}
	int getInputDevice(const std::string& tName) {
		int numDevices = Pa_GetDeviceCount();
		if (numDevices < 0) {
			LOGF_ERRR("PortAudio error getting device count: %i", numDevices);
			return -1;
		}

		for (int i = 0;i < numDevices;i++) {
			const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
			if(deviceInfo->maxInputChannels > 0)
				if(std::string(deviceInfo->name).find(tName)!=std::string::npos)
					return i;
		}
		return -1;
	}

	static int streamCallback(const void* inBuf, void* outBuf, ulong framesPerBuf, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags flags, void* userData) {
		auto* data = static_cast<AudioData*>(userData);
		const float* in = static_cast<const float*>(inBuf);
		if(inBuf == nullptr || reciever.muted) return paContinue;
		for (uint i = 0;i < framesPerBuf;i++) {
			data->in->data[i] = in[i];
		}
		smpl_t rms = aubio_level_lin(data->in);
		rms *= reciever.amplifier;
		if(rms<0) rms=0;
		else if(rms>1) rms=1;
		smpl_t db = 20.f * std::log10(rms + 0.00001f); 

		aubio_pitch_do(data->pitchProcessor, data->in, data->out);
		float pitchHz = data->out->data[0];

		data->pitch=pitchHz;
		data->rms=rms;
		data->dB=db;

		return paContinue;
	}

	bool initialize() {
		reciever.streamOpen = false;
		reciever.data.pitchProcessor = new_aubio_pitch("yinfft", BUFFER_SIZE, HOP_SIZE, SAMPLE_RATE);
		reciever.data.in = new_fvec(HOP_SIZE);
		reciever.data.out = new_fvec(1);
		aubio_pitch_set_unit(reciever.data.pitchProcessor, "Hz");
		if (Pa_Initialize() != paNoError) {
			LOG_ERRR("Failed to initialize PortAudio");
			return false;
		}
		LOG_INFO("Initialized PortAudio");
		return true;
	}

	bool create(const std::string& tName="") {
		PaStreamParameters streamParams{};
		PaDeviceIndex id = Pa_GetDefaultInputDevice();
		if (tName!="") {
			int gid = getInputDevice(tName);
			if (gid != -1) id = gid;
		}
		streamParams.device = id;
		if (streamParams.device == paNoDevice) {
			LOG_ERRR("Failed to detect audio input device");
			return false;
		}
		reciever.name=Pa_GetDeviceInfo(streamParams.device)->name;
		LOGF_INFO("Found audio device: %s", reciever.name);
		streamParams.channelCount = 1;
		streamParams.sampleFormat = paFloat32;
		streamParams.suggestedLatency = Pa_GetDeviceInfo(streamParams.device)->defaultLowInputLatency;
		streamParams.hostApiSpecificStreamInfo = nullptr;

		if (Pa_OpenStream(&reciever.stream, &streamParams, nullptr, SAMPLE_RATE, HOP_SIZE, paClipOff, streamCallback, &reciever.data) != paNoError) {
			LOG_ERRR("Failed to open input stream");
			return false;
		}
		LOG_INFO("Opened audio stream");
		reciever.streamOpen = true;

		return true;
	}

	void start() {
		Pa_StartStream(reciever.stream);
		LOG_INFO("Started audio stream");
	}

	void stop() {
		Pa_StopStream(reciever.stream);
		LOG_INFO("Stopped audio stream");
	}

	void close() {
		reciever.streamOpen = false;
		Pa_CloseStream(reciever.stream);
		LOG_INFO("Closed audio stream");
		reciever.stream = nullptr;
	}

	void remove() {
		stop();
		close();
		Pa_Terminate();
		del_aubio_pitch(reciever.data.pitchProcessor);
		del_fvec(reciever.data.in);
		del_fvec(reciever.data.out);
		LOG_INFO("Terminated PortAudio");
	}
}