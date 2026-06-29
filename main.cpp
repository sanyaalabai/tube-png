#include <firesteel/firesteel.hpp>
using namespace Firesteel;
#include <firesteel/utils/os.hpp>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include "microphone_sampler.hpp"

struct AvatarLayer {
public:
	bool enabled = true;
	std::string name = "Layer";
	std::string path = "";
	bool showOnTalking = false;
	bool showOnBlinking = false;
};

Entity box{glm::vec3(0, 0, -5), glm::vec3(45, 45, 0)};
std::shared_ptr<Shader> shader;
Camera camera{glm::vec3(0), glm::vec3(0, 0, -90)};
std::vector<const PaDeviceInfo*> inputDevices;

std::string avatarPath="my_avatar.tubepng.json";
std::vector<std::string> viableAvatarFormats = {
	"Uncompressed avatar (*.tubepng.json)", "*.tubepng.json",
};
std::vector<AvatarLayer> layers;
uint selectedSpriteId = 0;
std::vector<std::string> viableImageFormats = {
	"PNG (*.png)", "*.png",
	"JPG (*.jpg *.jpeg)", "*.jpg *.jpeg",
	"All files", "*",
};

class TubePngApp : public App {
	void onInitialize() override {
		box.load("res\\box.obj");
		shader=std::make_shared<Shader>("res/shader.vs", "res/shader.fs");
		box.setMaterialsShader(shader);
		camera.update();
		AudioIO::initialize();
		AudioIO::create();
		AudioIO::start();
		inputDevices=AudioIO::getInputDevices();
	}
	
	void onUpdate() override {
		camera.aspect = window.aspect();
		glm::mat4 projection = camera.getProjection(),
			view = camera.getView();
		shader->enable();
		shader->setMat4("projection", projection);
		shader->setMat4("view", view);
		box.draw();
		box.transform.rotation.x+=0.1f;
		box.transform.rotation.y+=0.1f;
		if(box.transform.rotation.x > 360.f) box.transform.rotation.x-=360.f;
		if(box.transform.rotation.y > 360.f) box.transform.rotation.y-=360.f;

		if(AudioIO::reciever.data.dB>=AudioIO::reciever.cutOff) {
			box.transform.size.x = 1.2f;
			box.transform.size.y = 1.2f;
			box.transform.size.z = 1.2f;
		} else {
			box.transform.size.x = 1.f;
			box.transform.size.y = 1.f;
			box.transform.size.z = 1.f;
		}

		drawUI();
	}
	void drawUI() {
		ImGui::Begin("Main");
		ImGui::Text("Microphone");
		if (ImGui::CollapsingHeader(Log::formatStr("%s", AudioIO::reciever.name).c_str())) {
			for (uint i = 0; i < inputDevices.size(); i++) {
				const PaDeviceInfo* device = inputDevices[i];
				if (AudioIO::reciever.name == device->name) continue;
				if (ImGui::MenuItem(Log::formatStr("%s##%i", device->name, i).c_str())) {
					LOGF("Selected device: %s", device->name);
					AudioIO::stop();
					AudioIO::close();
					AudioIO::create(device->name);
				}
			}
		}
		ImGui::SameLine();
		if(ImGui::Button("Refresh")) inputDevices = AudioIO::getInputDevices();
		ImGui::BeginDisabled();
		ImGui::SliderFloat("Volume (dB)", &AudioIO::reciever.data.dB, -100, 0);
		ImGui::SliderFloat("Pitch", &AudioIO::reciever.data.pitch, 0, 1000);
		ImGui::EndDisabled();
		ImGui::SliderFloat("Cut Off", &AudioIO::reciever.cutOff, -100, 0);
		ImGui::SliderFloat("Amplifier", &AudioIO::reciever.amplifier, 0, 2);
		ImGui::Text("Avatar");
		ImGui::BeginDisabled();
		ImGui::InputText("##save_avatar_path", &avatarPath);
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("...##save_avatar")) {
			auto r = OS::fileDialog(true, false, "", &viableAvatarFormats, "Select avatar save location");
			if (r.size() > 0) {
				avatarPath = r[0];
			}
		}
		ImGui::Text("Layers");
		for (uint i = 0; i < layers.size(); i++) {
			AvatarLayer& layer = layers[i];
			if(ImGui::MenuItem((layer.name+"##layer_"+std::to_string(i)).c_str())) selectedSpriteId = i;
		}
		if (ImGui::Button("+ Add layer")) {
			layers.push_back(AvatarLayer{ true, "Layer "+std::to_string(layers.size()),"" });
		}
		if (layers.size() > 0 && selectedSpriteId < layers.size()) {
			AvatarLayer& layer = layers[selectedSpriteId];
			ImGui::Text("Sprite layer");
			ImGui::InputText("##sprite_name", &layer.name);
			ImGui::BeginDisabled();
			ImGui::InputText("##select_sprite_path", &layer.path);
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("...##select_sprite")) {
				auto r = OS::fileDialog(false, false, "", &viableImageFormats, "Select layer sprite");
				if (r.size() > 0) {
					layer.path = r[0];
				}
			}
		}
		ImGui::End();
	}
	
	void onShutdown() override {
		AudioIO::remove();
		box.remove();
		shader->remove();
	}
};

int main() {
	return TubePngApp{}.start("tube.png");
}