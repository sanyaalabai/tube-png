#include <firesteel/firesteel.hpp>
using namespace Firesteel;
#include <firesteel/util/os.hpp>
#include <firesteel/util/geometry.hpp>
#include <firesteel/util/stbi_global.hpp>
#include <firesteel/util/imgui_utils.hpp>
#include "microphone_sampler.hpp"

enum AffectionState {
	AS_UNAFFECTED,
	AS_ON_TRUE,
	AS_ON_FALSE
};
static std::string affectionStateToStr(const AffectionState& v) {
	if(v==0) return "none";
	if(v==1) return "on_true";
	return "on_false";
}
static AffectionState strToAffectionState(const std::string& v) {
	if(v=="on_true") return AS_ON_TRUE;
	if(v=="on_false") return AS_ON_FALSE;
	return AS_UNAFFECTED;
}

struct AvatarLayer {
public:
	bool enabled=true;
	std::string name="Layer";
	Texture texture{};
	glm::vec3 position{0,0,-2};
	glm::vec3 rotation{};
	glm::vec2 size{1};
	AffectionState showOnTalking=AS_UNAFFECTED;
	AffectionState showOnBlinking=AS_UNAFFECTED;
};

Entity box{glm::vec3(0, 0, -5), glm::vec3(45, 45, 0)};
bool showBox=true;
bool haveAnyLayerSelected=false;
bool fakeThresholdForSmoothTalking=true;
int fakeThresholdForSmoothTalkingMs=500;
int curThresholdForSmoothTalkingMs=0;
bool transparentWindow=true;
Entity spriteQuad{};
std::shared_ptr<Shader> shader;
Camera camera{glm::vec3(0), glm::vec3(0, 0, -90)};

std::string avatarPath="my_avatar.tubepng.json";
std::vector<std::string> viableAvatarFormats = {
	"Uncompressed avatar (*.tubepng.json)", "*.tubepng.json",
};
std::vector<AvatarLayer> layers;
uint selectedSpriteId = 0;
std::vector<std::string> viableImageFormats = {
	"PNG (*.png)", "*.png",
	"JPG (*.jpg *.jpeg)", "*.jpg *.jpeg",
};
std::vector<const PaDeviceInfo*> inputDevices;

class TubePngApp : public App {
	void onInitialize() override {
		if(!std::filesystem::exists("avatars")) std::filesystem::create_directory("avatars");
		box.load("res\\box.obj");
		spriteQuad.load("res\\quad.obj");
		//spriteQuad.pushMesh(Geometry::quad());
		shader=std::make_shared<Shader>("res/shader.vs", "res/shader.fs");
		box.setMaterialsShader(shader);
		spriteQuad.replaceMaterials(std::make_shared<Material>(), true);
		spriteQuad.setMaterialsShader(shader);
		camera.update();
		loadConfig("config.json");
		if(std::filesystem::exists(avatarPath)) loadAvatar(avatarPath);
		AudioIO::initialize();
		AudioIO::create(AudioIO::reciever.name);
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
		if(showBox) {
			box.draw();
			box.transform.rotation.x+=0.1f;
			box.transform.rotation.y+=0.1f;
			if(box.transform.rotation.x > 360.f) box.transform.rotation.x-=360.f;
			if(box.transform.rotation.y > 360.f) box.transform.rotation.y-=360.f;
		}
		
		bool talking=AudioIO::reciever.data.dB>=AudioIO::reciever.cutOff;
		if(fakeThresholdForSmoothTalking) {
			if(talking) curThresholdForSmoothTalkingMs=fakeThresholdForSmoothTalkingMs;
			else talking = curThresholdForSmoothTalkingMs > 0;
			curThresholdForSmoothTalkingMs -= 1;
		}

		for(uint i=0;i<layers.size();i++) {
			auto& layer=layers[i];
			if(!layer.enabled) continue;
			if(layer.showOnTalking==AS_ON_TRUE && !talking) continue;
			if(layer.showOnTalking==AS_ON_FALSE && talking) continue;
			spriteQuad.transform.position = layer.position;
			spriteQuad.transform.rotation = layer.rotation;
			spriteQuad.transform.size = { layer.size, 1 };
			layer.texture.bind();
			shader->setInt("material.diffuse0", 0);
			shader->setVec3("material.diffuse", glm::vec3(1));
			shader->setMat4("model", spriteQuad.transform.getMatrix());
			
			spriteQuad.model.meshes[0].draw(NULL, true);
		}

		if(AudioIO::reciever.data.dB>=AudioIO::reciever.cutOff) {
			spriteQuad.transform.size.x = 1.2f;
			spriteQuad.transform.size.y = 1.2f;
			spriteQuad.transform.size.z = 1.2f;
		} else {
			spriteQuad.transform.size.x = 1.f;
			spriteQuad.transform.size.y = 1.f;
			spriteQuad.transform.size.z = 1.f;
		}

		drawUI();
	}
	void drawUI() {
		ImGui::Begin("Main");
		if(ImGui::BeginPopupModal("Select Microphone")) {
			if(ImGui::Button("Refresh")) inputDevices=AudioIO::getInputDevices();
			for(uint i=0;i<inputDevices.size();i++) {
				const PaDeviceInfo* device=inputDevices[i];
				if(AudioIO::reciever.name==device->name) continue;
				if(ImGui::MenuItem(Log::formatStr("%s##%i", device->name, i).c_str())) {
					LOGF("Selected device: %s", device->name);
					AudioIO::stop();
					AudioIO::close();
					AudioIO::create(device->name);
					ImGui::CloseCurrentPopup();
				}
			}
			if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
		ImGui::Checkbox("Show box", &showBox);
		if(ImGui::Checkbox("Transparent background", &transparentWindow)) {
			glm::vec3 bgc = window.getClearColor();
			window.setClearColor({ bgc, !transparentWindow });
		}
		if(!transparentWindow) {
			glm::vec3 bgc = window.getClearColor();
			if(ImGui::ColorEdit3("Background", &bgc)) window.setClearColor({ bgc, 1 });
		}
		if(ImGui::CollapsingHeader("Microphone")) {
			ImGui::Text(Log::formatStr("%s", AudioIO::reciever.name).c_str());
			ImGui::SameLine();
			if(ImGui::Button((AudioIO::reciever.muted?"U##mute_microphone":"M##mute_microphone"))) {
				AudioIO::reciever.muted=!AudioIO::reciever.muted;
				if(AudioIO::reciever.muted) {
					AudioIO::reciever.data.dB=-100;
					AudioIO::reciever.data.rms=0;
					AudioIO::reciever.data.pitch=0;
				}
			}
			ImGui::SameLine();
			if(ImGui::Button("...##select_microphone")) ImGui::OpenPopup("Select Microphone");
			if(AudioIO::reciever.muted) ImGui::Text("Your microphone is muted");
			ImGui::Text("Info");
			ImGui::BeginDisabled();
			ImGui::SliderFloat("Volume (dB)", &AudioIO::reciever.data.dB, -100, 0);
			ImGui::SliderFloat("Pitch", &AudioIO::reciever.data.pitch, 0, 1000);
			ImGui::EndDisabled();
			ImGui::Text("Talking");
			ImGui::SliderFloat("Cut Off", &AudioIO::reciever.cutOff, -100, 0);
			ImGui::SliderFloat("Amplifier", &AudioIO::reciever.amplifier, 0, 5);
			ImGui::Text("Threshold faker");
			ImGui::Checkbox("Enabled", &fakeThresholdForSmoothTalking);
			if(fakeThresholdForSmoothTalking) ImGui::SliderInt("Delay (ms)", &fakeThresholdForSmoothTalkingMs, 0, 10000);
		}
		if(ImGui::CollapsingHeader("Avatar Data")) {
			ImGui::BeginDisabled();
			ImGui::InputText("Path##save_avatar_path", &avatarPath);
			ImGui::EndDisabled();
			if (ImGui::Button("Save##save_avatar")) {
				if(avatarPath.empty()||!std::filesystem::exists(avatarPath)) {
					auto r = OS::fileDialog(true, false, "", &viableAvatarFormats, "Select avatar save location");
					if (r.size() > 0) {
						avatarPath = r[0];
						saveAvatar(avatarPath);
					}
				} else saveAvatar(avatarPath);
			}
			ImGui::SameLine();
			if (ImGui::Button("Save as...##save_as_avatar")) {
				auto r = OS::fileDialog(true, false, "", &viableAvatarFormats, "Select avatar save location");
				if (r.size() > 0) {
					avatarPath = r[0];
					saveAvatar(avatarPath);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Load##load_avatar")) {
				auto r = OS::fileDialog(false, false, "", &viableAvatarFormats, "Select avatar load location");
				if (r.size() > 0) {
					avatarPath = r[0];
					loadAvatar(avatarPath);
				}
			}
		}
		if(ImGui::CollapsingHeader("Layers")) {
			for (uint i = 0; i < layers.size(); i++) {
				AvatarLayer& layer = layers[i];
				if(ImGui::MenuItem((layer.name+"##layer_"+std::to_string(i)).c_str(), NULL, selectedSpriteId==i && haveAnyLayerSelected)) {
					selectedSpriteId = i;
					haveAnyLayerSelected = true;
				}
			}
			if(ImGui::Button("+ Add layer")) {
				layers.push_back(AvatarLayer{ true, "Layer " + std::to_string(layers.size()) });
			}
		}
		if (layers.size() > 0 && selectedSpriteId < layers.size() && haveAnyLayerSelected) {
			if(ImGui::CollapsingHeader("Sprite layer")) {
				AvatarLayer& layer = layers[selectedSpriteId];
				ImGui::Checkbox("##layer_enabled", &layer.enabled);
				ImGui::SameLine();
				ImGui::InputText("##layer_name", &layer.name);
				ImGui::SameLine();
				if(ImGui::Button("X##layer_delete")) {
					layers.erase(layers.begin()+selectedSpriteId);
					selectedSpriteId=0;
					haveAnyLayerSelected = false;
				}
				ImGui::BeginDisabled();
				ImGui::InputText("##select_sprite_path", &layer.texture.path);
				ImGui::EndDisabled();
				ImGui::SameLine();
				if (ImGui::Button("...##select_sprite")) {
					auto r = OS::fileDialog(false, false, "", &viableImageFormats, "Select layer sprite");
					if (r.size() > 0) {
						layer.texture.path=r[0];
						loadLayerTexture(layer);
					}
				}
				ImGui::Separator();
				ImGui::DragFloat3("Position", &layer.position);
				ImGui::DragFloat3("Rotation", &layer.rotation);
				ImGui::DragFloat2("Scale", &layer.size);
				ImGui::Separator();
				int s = layer.showOnTalking;
				if(ImGui::SliderInt("Show on talking", &s, 0, 2))
					layer.showOnTalking = static_cast<AffectionState>(s);
			}
		}
		ImGui::End();
	}

	void loadLayerTexture(AvatarLayer& tLayer) {
		tLayer.texture.remove();
		bool mono = false;
		tLayer.texture = Texture{ TextureFromFile(tLayer.texture.path, &mono, true), TT_DIFFUSE, tLayer.texture.path, mono };
	}
	void saveAvatar(const std::string& tPath) {
		nlohmann::json scene;
		scene["ver"]=1;
		for (uint i=0;i<layers.size();i++) {
			auto& layer=layers[i];
			scene["layers"][i]["enabled"]=layer.enabled;
			scene["layers"][i]["name"]=layer.name;
			scene["layers"][i]["tex"]=layer.texture.path;

			scene["layers"][i]["position"]["x"]=layer.position.x;
			scene["layers"][i]["position"]["y"]=layer.position.y;
			scene["layers"][i]["position"]["z"]=layer.position.z;
			scene["layers"][i]["rotation"]["x"]=layer.rotation.x;
			scene["layers"][i]["rotation"]["y"]=layer.rotation.y;
			scene["layers"][i]["rotation"]["z"]=layer.rotation.z;
			scene["layers"][i]["scale"]["x"]=layer.size.x;
			scene["layers"][i]["scale"]["y"]=layer.size.y;

			scene["layers"][i]["on_talking"]=affectionStateToStr(layer.showOnTalking);
		}
		std::ofstream o(tPath);
		o << scene << std::endl;
		o.close();
		LOG_INFO("Saved current avatar to: \"" + tPath + "\"");
	}
	bool loadAvatar(const std::string& tPath) {
		if(!std::filesystem::exists(tPath)) {
			LOG_ERRR("Avatar at path \"" + tPath + "\" doesn't exist");
			return false;
		}
		for(uint i=0;i<layers.size();i++) {
			auto& layer = layers[i];
			layer.texture.remove();
		}
		layers.clear();
		std::ifstream ifs(tPath);
		try {
			nlohmann::json scene = nlohmann::json::parse(ifs);
			ifs.close();
			if (scene.contains("layers"))
				for (uint i=0;i<scene["layers"].size();i++) {
					nlohmann::json local = scene["layers"][i];
					AvatarLayer layer{};
					if(local.contains("enabled")) layer.enabled = local["enabled"];
					if(local.contains("name")) layer.name = local["name"];
					if(local.contains("tex")) {
						layer.texture.path = local["tex"];
						loadLayerTexture(layer);
					}

					if(local.contains("position")) if(local["position"].size() == 3)
						layer.position = { local["position"]["x"], local["position"]["y"], local["position"]["z"] };
					if(local.contains("rotation")) if(local["rotation"].size() == 3)
						layer.rotation = { local["rotation"]["x"], local["rotation"]["y"], local["rotation"]["z"] };
					if(local.contains("scale")) if(local["scale"].size() == 2)
						layer.size = { local["scale"]["x"], local["scale"]["y"] };

					if(local.contains("on_talking")) layer.showOnTalking = strToAffectionState(local["on_talking"]);
					layers.emplace_back(layer);
				}
		} catch(const std::runtime_error& e) {
			LOGF_ERRR("Failed to parse avatar: %s", e.what());
			return false;
		}
		LOG_INFO("Loaded avatar from: \"" + tPath + "\"");
		return true;
	}

	void saveConfig(const std::string& tPath) {
		nlohmann::json cfg;
		cfg["micro"]["amp"] = AudioIO::reciever.amplifier;
		cfg["micro"]["cut_off"] = AudioIO::reciever.cutOff;
		cfg["micro"]["name"] = AudioIO::reciever.name;
		cfg["faker"]["enabled"] = fakeThresholdForSmoothTalking;
		cfg["faker"]["ms"] = fakeThresholdForSmoothTalkingMs;
		cfg["show_box"] = showBox;
		cfg["background"]["color"] = { window.getClearColor().r, window.getClearColor().g, window.getClearColor().b, window.getClearColor().a };
		cfg["avatars"]["last"] = avatarPath;
		std::ofstream o(tPath);
		o << cfg << std::endl;
		o.close();
		LOG_INFO("Saved config to: \"" + tPath + "\"");
	}
	bool loadConfig(const std::string& tPath) {
		if(!std::filesystem::exists(tPath)) {
			LOG_ERRR("Avatar at path \"" + tPath + "\" doesn't exist");
			return false;
		}
		for(uint i=0;i<layers.size();i++) {
			auto& layer = layers[i];
			layer.texture.remove();
		}
		layers.clear();
		std::ifstream ifs(tPath);
		try {
			nlohmann::json cfg = nlohmann::json::parse(ifs);
			ifs.close();
			if(cfg.contains("micro")) {
				if(cfg["micro"].contains("amp")) AudioIO::reciever.amplifier = cfg["micro"]["amp"];
				if(cfg["micro"].contains("cut_off")) AudioIO::reciever.cutOff = cfg["micro"]["cut_off"];
				if(cfg["micro"].contains("name")) AudioIO::reciever.name = cfg["micro"]["name"].get<std::string>().c_str();
			}
			if(cfg.contains("faker")) {
				if(cfg["faker"].contains("enabled")) fakeThresholdForSmoothTalking = cfg["faker"]["enabled"];
				if(cfg["faker"].contains("ms")) fakeThresholdForSmoothTalkingMs = cfg["faker"]["ms"];
			}
			if(cfg.contains("show_box")) showBox = cfg["show_box"];
			if(cfg.contains("avatars")) {
				if(cfg["avatars"].contains("last")) avatarPath = cfg["avatars"]["last"];
			}
			if(cfg.contains("background")) {
				if(cfg["background"].contains("color")) if(cfg["background"]["color"].size() == 4) {
					window.setClearColor({ cfg["background"]["color"][0], cfg["background"]["color"][1], cfg["background"]["color"][2], cfg["background"]["color"][3] });
					transparentWindow=cfg["background"]["color"][3]==0;
				}
			}
		} catch(const std::runtime_error& e) {
			LOGF_ERRR("Failed to parse config: %s", e.what());
			return false;
		}
		LOG_INFO("Loaded config from: \"" + tPath + "\"");
		return true;
	}
	
	void onShutdown() override {
		AudioIO::remove();
		for(uint i=0;i<layers.size();i++) {
			auto& layer = layers[i];
			layer.texture.remove();
		}
		layers.clear();
		box.remove();
		spriteQuad.remove();
		shader->remove();
		saveConfig("config.json");
	}
};

int main() {
	return TubePngApp{}.start("tube.png", 800, 600, WS_NORMAL, WFT_RGB_TRANSPARENT);
}