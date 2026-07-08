#define FS_NO_COMPONENTS
#include <firesteel/firesteel.hpp>
using namespace Firesteel;
#include <firesteel/input/keyboard.hpp>
#include <firesteel/util/delta_time.hpp>
#include <firesteel/util/os.hpp>
#include <firesteel/util/geometry.hpp>
#include <firesteel/util/stbi_global.hpp>
#include <firesteel/util/imgui_utils.hpp>
#include <firesteel/util/easing.hpp>
#include "microphone_sampler.hpp"
#include "embedded.hpp"

enum AffectionState {
	AS_UNAFFECTED,
	AS_ON_TRUE,
	AS_ON_FALSE,
	_SIZE
};
using EasingType = float(*)(float);
float linearInOut(float v) { return v>0.5f?2-v*2:v*2; }
EasingType easingsInOut[] = {
	linearInOut,
	Easing::quadInOut,
	Easing::cubicInOut,
	Easing::quartInOut,
	Easing::sineInOut,
	Easing::expoInOut,
	Easing::circInOut,
	Easing::bounceInOut,
	Easing::elasticInOut
};
static const char* easingInOutToStr(const uint& v) {
	if(v==0) return "None";
	if(v==1) return "Linear";
	if(v==2) return "Quad";
	if(v==3) return "Cubic";
	if(v==4) return "Quart";
	if(v==5) return "Sine";
	if(v==6) return "Expo";
	if(v==7) return "Circ";
	if(v==8) return "Bounce";
	return "Elastic";
}
static const char* affectionStateToStr(const AffectionState& v) {
	if(v==0) return "none";
	if(v==1) return "on_true";
	return "on_false";
}
const char* affectionStateToGoodStr(AffectionState v) {
	if(v == 0) return "Unaffected";
	if(v == 1) return "On True";
	return "On False";
}
const char* asStrPhysical(AffectionState v) {
	if(v == 0) return "Unaffected";
	if(v == 1) return "When open";
	return "When closed";
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

	glm::vec3 rotationTalkChange{};
	glm::vec3 positionTalkChange{};
	glm::vec2 sizeTalkChange{};
	bool temporal=false;
	uint selectedEasing=0;
	uint easingTimeMs=200;
	uint curEasingTime=0;

	bool idleEnabled=false;
	glm::vec3 rotationIdleChange{};
	glm::vec3 positionIdleChange{};
	glm::vec2 sizeIdleChange{};
	uint selectedIdleEasing=0;

	AffectionState showOnTalking=AS_UNAFFECTED;
	uint blinkingLayerId=0;
	AffectionState showOnBlinking=AS_UNAFFECTED;
};
struct BlinkingLayer {
public:
	uint current=0;
	uint blinkCooldown=1000;
	uint blinkTime=200;
	uint offset=0;
};

bool haveAnyLayerSelected=false;
bool fakeThresholdForSmoothTalking=true;
int fakeThresholdForSmoothTalkingMs=500;
int curThresholdForSmoothTalkingMs=0;
bool calcBlinking=true;
bool transparentWindow=true;
bool showAllRotationDirs=false;
Entity spriteQuad{};
std::shared_ptr<Shader> shader;
Camera camera{glm::vec3(0), glm::vec3(0, 0, -90)};

bool hideImGui=false;
bool layerInspectorWindowOpen=false;
bool configOpen=false;

std::string avatarPath="my_avatar.tubepng.json";
std::vector<std::string> viableAvatarFormats = {
	"Uncompressed avatar (*.tubepng.json)", "*.tubepng.json",
};
std::vector<AvatarLayer> layers;
std::vector<BlinkingLayer> blinkingLayers;
uint selectedSpriteId=0;
std::vector<std::string> viableImageFormats = {
	"PNG (*.png)", "*.png",
	"JPG (*.jpg *.jpeg)", "*.jpg *.jpeg",
};
std::vector<const PaDeviceInfo*> inputDevices;

class TubePngApp : public App {
	void loadLayerTexture(AvatarLayer& tLayer) {
		tLayer.texture.destroy();
		bool mono = false;
		tLayer.texture = Texture{ TextureFromFile(tLayer.texture.path, &mono, true), TT_DIFFUSE, tLayer.texture.path, mono };
	}

	void saveAvatar(std::string& tPath) {
		nlohmann::json scene;
		scene["ver"]=1;
		for(uint i=0;i<layers.size();i++) {
			auto& layer=layers[i];
			scene["layers"][i]["enabled"]=layer.enabled;
			scene["layers"][i]["name"]=layer.name;
			scene["layers"][i]["tex"]=layer.texture.path;

			if(layer.position!=glm::vec3(0,0,-2)) {
				scene["layers"][i]["position"][0]=layer.position.x;
				scene["layers"][i]["position"][1]=layer.position.y;
				scene["layers"][i]["position"][2]=layer.position.z;
			}
			if(layer.rotation!=glm::vec3(0)) {
				scene["layers"][i]["rotation"][0]=layer.rotation.x;
				scene["layers"][i]["rotation"][1]=layer.rotation.y;
				scene["layers"][i]["rotation"][2]=layer.rotation.z;
			}
			if(layer.size!=glm::vec2(1)) {
				scene["layers"][i]["scale"][0]=layer.size.x;
				scene["layers"][i]["scale"][1]=layer.size.y;
			}

			if(layer.positionTalkChange!=glm::vec3(0)) {
				scene["layers"][i]["on_talk"]["position"][0] = layer.positionTalkChange.x;
				scene["layers"][i]["on_talk"]["position"][1] = layer.positionTalkChange.y;
				scene["layers"][i]["on_talk"]["position"][2] = layer.positionTalkChange.z;
			}
			if(layer.rotationTalkChange!=glm::vec3(0)) {
				scene["layers"][i]["on_talk"]["rotation"][0] = layer.rotationTalkChange.x;
				scene["layers"][i]["on_talk"]["rotation"][1] = layer.rotationTalkChange.y;
				scene["layers"][i]["on_talk"]["rotation"][2] = layer.rotationTalkChange.z;
			}
			if(layer.sizeTalkChange!=glm::vec2(0)) {
				scene["layers"][i]["on_talk"]["scale"][0] = layer.sizeTalkChange.x;
				scene["layers"][i]["on_talk"]["scale"][1] = layer.sizeTalkChange.y;
			}
			if(layer.temporal) scene["layers"][i]["on_talk"]["temp"]=true;
			if(layer.selectedEasing!=0) scene["layers"][i]["on_talk"]["ease"]=layer.selectedEasing;
			if(layer.easingTimeMs!=200) scene["layers"][i]["on_talk"]["ease_time"]=layer.easingTimeMs;

			if(layer.idleEnabled) scene["layers"][i]["idle"]["enabled"]=true;
			if(layer.positionIdleChange!=glm::vec3(0)) {
				scene["layers"][i]["idle"]["position"][0] = layer.positionIdleChange.x;
				scene["layers"][i]["idle"]["position"][1] = layer.positionIdleChange.y;
				scene["layers"][i]["idle"]["position"][2] = layer.positionIdleChange.z;
			}
			if(layer.rotationIdleChange!=glm::vec3(0)) {
				scene["layers"][i]["idle"]["rotation"][0] = layer.rotationIdleChange.x;
				scene["layers"][i]["idle"]["rotation"][1] = layer.rotationIdleChange.y;
				scene["layers"][i]["idle"]["rotation"][2] = layer.rotationIdleChange.z;
			}
			if(layer.sizeIdleChange!=glm::vec2(0)) {
				scene["layers"][i]["idle"]["scale"][0] = layer.sizeIdleChange.x;
				scene["layers"][i]["idle"]["scale"][1] = layer.sizeIdleChange.y;
			}
			if(layer.selectedIdleEasing!=0) scene["layers"][i]["idle"]["ease"] = layer.selectedIdleEasing;

			if(layer.showOnTalking!=AS_UNAFFECTED) scene["layers"][i]["visibility"]["on_talking"]=affectionStateToStr(layer.showOnTalking);
			if(layer.showOnBlinking!=AS_UNAFFECTED) {
				scene["layers"][i]["visibility"]["on_blinking"] = affectionStateToStr(layer.showOnBlinking);
				scene["layers"][i]["visibility"]["blinking_layer"] = layer.blinkingLayerId;
			}
		}
		uint realBlinkSize=static_cast<uint>(blinkingLayers.size());
		for(uint i=static_cast<uint>(blinkingLayers.size());i>0;i--) {
			auto& layer=blinkingLayers[static_cast<size_t>(i-1)];
			if(layer.blinkCooldown==1000 && layer.blinkTime==200 && layer.offset==0) realBlinkSize--;
			else break;
		}
		if(realBlinkSize>16) realBlinkSize=0;
		for(uint i=0;i<realBlinkSize;i++) {
			auto& layer=blinkingLayers[i];
			if(layer.blinkCooldown==1000 && layer.blinkTime==200 && layer.offset==0) {
				scene["blinking_layers"][i]=nlohmann::json::object();
				continue;
			}
			scene["blinking_layers"][i]["cooldown"]=layer.blinkCooldown;
			scene["blinking_layers"][i]["time"]=layer.blinkTime;
			scene["blinking_layers"][i]["offset"]=layer.offset;
		}
		if(tPath.find(".tubepng.json")==std::string::npos) tPath+=".tubepng.json";
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
			layer.texture.destroy();
		}
		layers.clear();
		std::ifstream ifs(tPath);
		try {
			nlohmann::json scene = nlohmann::json::parse(ifs);
			ifs.close();
			if(scene.contains("layers"))
				for(uint i=0;i<scene["layers"].size();i++) {
					nlohmann::json local = scene["layers"][i];
					AvatarLayer layer{};
					if(local.contains("enabled")) layer.enabled = local["enabled"];
					if(local.contains("name")) layer.name = local["name"];
					if(local.contains("tex")) {
						layer.texture.path = local["tex"];
						loadLayerTexture(layer);
					}

					if(local.contains("position")) if (local["position"].size()==3)
						layer.position = { local["position"][0], local["position"][1], local["position"][2] };
					if(local.contains("rotation")) if(local["rotation"].size()==3)
						layer.rotation = { local["rotation"][0], local["rotation"][1], local["rotation"][2] };
					if(local.contains("scale")) if(local["scale"].size() == 2)
						layer.size = { local["scale"][0], local["scale"][1] };

					if(local.contains("on_talk")) {
						if(local["on_talk"].contains("position")) if (local["on_talk"]["position"].size() == 3)
							layer.positionTalkChange = { local["on_talk"]["position"][0], local["on_talk"]["position"][1], local["on_talk"]["position"][2] };
						if(local["on_talk"].contains("rotation")) if(local["on_talk"]["rotation"].size() == 3)
							layer.rotationTalkChange = { local["on_talk"]["rotation"][0], local["on_talk"]["rotation"][1], local["on_talk"]["rotation"][2] };
						if(local["on_talk"].contains("scale")) if(local["on_talk"]["scale"].size() == 2)
							layer.sizeTalkChange = { local["on_talk"]["scale"][0], local["on_talk"]["scale"][1] };
						if(local["on_talk"].contains("ease")) layer.selectedEasing=local["on_talk"]["ease"];
						if(local["on_talk"].contains("ease_talk")) layer.easingTimeMs=local["on_talk"]["ease_talk"];
						if(local["on_talk"].contains("temp")) layer.temporal=local["on_talk"]["temp"];
					}

					if(local.contains("idle")) {
						if(local["idle"].contains("enabled")) layer.idleEnabled=local["idle"]["enabled"];
						if(local["idle"].contains("position")) if(local["idle"]["position"].size() == 3)
							layer.positionIdleChange = { local["idle"]["position"][0], local["idle"]["position"][1], local["idle"]["position"][2] };
						if(local["idle"].contains("rotation")) if(local["idle"]["rotation"].size() == 3)
							layer.rotationIdleChange = { local["idle"]["rotation"][0], local["idle"]["rotation"][1], local["idle"]["rotation"][2] };
						if(local["idle"].contains("scale")) if(local["idle"]["scale"].size() == 2)
							layer.sizeIdleChange = { local["idle"]["scale"][0], local["idle"]["scale"][1] };
						if(local["idle"].contains("ease")) layer.selectedIdleEasing=local["idle"]["ease"];
					}

					if(local.contains("visibility")) {
						if(local["visibility"].contains("on_talking")) layer.showOnTalking = strToAffectionState(local["visibility"]["on_talking"]);
						if(local["visibility"].contains("on_blinking")) layer.showOnBlinking = strToAffectionState(local["visibility"]["on_blinking"]);
						if(local["visibility"].contains("blinking_layer")) layer.blinkingLayerId = local["visibility"]["blinking_layer"];
					}
					layers.emplace_back(layer);
				}
			if(scene.contains("blinking_layers"))
				for(uint i=0;i<scene["blinking_layers"].size();i++) {
					nlohmann::json local = scene["blinking_layers"][i];
					BlinkingLayer layer{};
					if(local.contains("cooldown")) layer.blinkCooldown=local["cooldown"];
					if(local.contains("time")) layer.blinkCooldown=local["time"];
					if(local.contains("offset")) layer.blinkCooldown=local["offset"];
					layer.current=layer.offset;
					blinkingLayers.push_back(layer);
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
		cfg["show_all_rotation_dirs"] = showAllRotationDirs;
		cfg["background"]["color"] = { window.getClearColor().r, window.getClearColor().g, window.getClearColor().b, window.getClearColor().a };
		cfg["avatar"]["blinking"] = calcBlinking;
		cfg["avatar"]["last"] = avatarPath;
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
			layer.texture.destroy();
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
			if(cfg.contains("show_all_rotation_dirs")) showAllRotationDirs = cfg["show_all_rotation_dirs"];
			if(cfg.contains("avatar")) {
				if(cfg["avatar"].contains("last")) avatarPath = cfg["avatar"]["last"];
				if(cfg["avatar"].contains("blinking")) calcBlinking = cfg["avatar"]["blinking"];
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

	void onInitialize() override {
		if(!std::filesystem::exists("avatars")) std::filesystem::create_directory("avatars");
		spriteQuad.load("res\\quad.obj");
		//spriteQuad.pushMesh(Geometry::quad());
		shader=std::make_shared<Shader>("res/shader.vs", "res/shader.fs");
		spriteQuad.replaceMaterials(std::make_shared<Material>(), true);
		spriteQuad.setMaterialsShader(shader);
		camera.update();
		blinkingLayers.resize(16);
		loadConfig("config.json");
		if(std::filesystem::exists(avatarPath)) loadAvatar(avatarPath);
		AudioIO::initialize();
		AudioIO::create(AudioIO::reciever.name);
		AudioIO::start();
		inputDevices=AudioIO::getInputDevices();
		if(std::filesystem::exists("res/font.ttf")) {
			ImGuiIO& io=ImGui::GetIO();
			ImFont* imguiFont=io.Fonts->AddFontFromFileTTF("res/font.ttf", 14.0f, NULL, io.Fonts->GetGlyphRangesCyrillic());
			if(imguiFont==NULL) {
				critShutdown(-18, "Couldn't load ImGui font");
				return;
			}
			io.FontDefault=imguiFont;
		}
		window.setIconFromMemory(ucDataBlock, ucDataBlockSize);

		auto& style=ImGui::GetStyle();
		ImVec4* colors = style.Colors;
		style.WindowPadding = ImVec2(12.0f, 12.0f);
		style.FramePadding = ImVec2(8.0f, 5.0f);
		style.ItemSpacing = ImVec2(10.0f, 6.0f);
		style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
		style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
		style.IndentSpacing = 18.0f;
		style.ScrollbarSize = 10.0f;
		style.GrabMinSize = 10.0f;
		style.WindowRounding = 0.0f;
		style.ChildRounding = 3.0f;
		style.FrameRounding = 3.0f;
		style.PopupRounding = 3.0f;
		style.ScrollbarRounding = 4.0f;
		style.GrabRounding = 2.0f;
		style.TabRounding = 3.0f;
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.TabBorderSize = 0.0f;
		colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.06f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.09f, 0.98f);
		colors[ImGuiCol_Border] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.40f, 0.42f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
		colors[ImGuiCol_Button] = ImVec4(0.10f, 0.35f, 0.32f, 1.00f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.14f, 0.45f, 0.41f, 1.00f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.08f, 0.28f, 0.26f, 1.00f);
		colors[ImGuiCol_CheckboxSelectedBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.18f, 0.55f, 0.50f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.14f, 0.45f, 0.41f, 1.00f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.18f, 0.55f, 0.50f, 1.00f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.10f, 0.35f, 0.32f, 0.40f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.06f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.05f, 0.06f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05f, 0.05f, 0.06f, 1.00f);
		colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.18f, 0.55f, 0.50f, 1.00f);
		colors[ImGuiCol_Tab] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
		colors[ImGuiCol_TabActive] = ImVec4(0.05f, 0.05f, 0.06f, 1.00f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.05f, 0.05f, 0.06f, 1.00f);
		colors[ImGuiCol_DockingPreview] = ImVec4(0.10f, 0.35f, 0.32f, 0.60f);
		colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.05f, 0.05f, 0.06f, 1.00f);
		colors[ImGuiCol_NavHighlight] = ImVec4(0.14f, 0.45f, 0.41f, 0.80f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.24f, 0.24f, 0.26f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.30f, 0.30f, 0.32f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.14f, 0.45f, 0.41f, 0.40f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.14f, 0.45f, 0.41f, 0.60f);
	}
	
	void onUpdate() override {
		if(Keyboard::keyDown(KeyCode::F1)) hideImGui=!hideImGui;
		bool ctrl=Keyboard::getKey(KeyCode::LEFT_CONTROL)||Keyboard::getKey(KeyCode::RIGHT_CONTROL);
		bool shift=Keyboard::getKey(KeyCode::LEFT_SHIFT)||Keyboard::getKey(KeyCode::RIGHT_SHIFT);
		if(ctrl) {
			if(Keyboard::keyDown(KeyCode::S)) {
				if(shift) techSaveAs();
				else techSave();
			}
			if(Keyboard::keyDown(KeyCode::O)) techLoad();
			if(Keyboard::keyDown(KeyCode::R)) techReload();
		}

		camera.aspect = window.aspect();
		glm::mat4 projection = camera.getProjection(),
			view = camera.getView();
		shader->enable();
		shader->setMat4("projection", projection);
		shader->setMat4("view", view);

		std::vector<bool> blinkingLayersCur;
		if(calcBlinking)
			for(uint i=0;i<blinkingLayers.size();i++) {
				auto& l=blinkingLayers[i];
				l.current+=1;
				bool b=l.current>=l.blinkCooldown;
				if(b) if(l.current>=(l.blinkCooldown+l.blinkTime)) {
					b=false;
					l.current=0;
				}
				blinkingLayersCur.push_back(b);
			}
		
		bool talking=AudioIO::reciever.data.dB>=AudioIO::reciever.cutOff;
		if(fakeThresholdForSmoothTalking) {
			if(talking) curThresholdForSmoothTalkingMs=fakeThresholdForSmoothTalkingMs;
			else talking=curThresholdForSmoothTalkingMs>0;
			curThresholdForSmoothTalkingMs-=1;
		}

		if(haveAnyLayerSelected) {
			glm::vec3 moveVec{0};
			if(Keyboard::getKey(KeyCode::LEFT)) moveVec.x-=0.1f;
			if(Keyboard::getKey(KeyCode::RIGHT)) moveVec.x+=0.1f;
			if(Keyboard::getKey(KeyCode::DOWN)) moveVec.y-=0.1f;
			if(Keyboard::getKey(KeyCode::UP)) moveVec.y+=0.1f;
			if(ctrl) moveVec*=2;
			if(shift) moveVec/=2;
			layers[selectedSpriteId].position+=moveVec*DeltaTime::get();
		}

		for(uint i=0;i<layers.size();i++) {
			auto& layer=layers[i];

			if(!layer.enabled) continue;
			if(layer.showOnTalking==AS_ON_TRUE && !talking) continue;
			if(layer.showOnTalking==AS_ON_FALSE && talking) continue;
			if(calcBlinking) {
				if(layer.showOnBlinking==AS_ON_TRUE && !blinkingLayersCur[layer.blinkingLayerId]) continue;
				if(layer.showOnBlinking==AS_ON_FALSE && blinkingLayersCur[layer.blinkingLayerId]) continue;
			}

			if(layer.selectedEasing==0 || layer.easingTimeMs<=0) {
				spriteQuad.transform.position = layer.position + ((talking)?layer.positionTalkChange:glm::vec3{});
				spriteQuad.transform.rotation = layer.rotation + ((talking)?layer.rotationTalkChange:glm::vec3{});
				spriteQuad.transform.size = glm::vec3(layer.size, 1) + ((talking)?glm::vec3(layer.sizeTalkChange,0):glm::vec3{});
			} else if(layer.temporal) {
				if(layer.curEasingTime<layer.easingTimeMs && talking) {
					auto& ease=easingsInOut[layer.selectedEasing-1];
					float apply=ease(linearInOut(static_cast<float>(layer.curEasingTime)/layer.easingTimeMs));
					spriteQuad.transform.position = layer.position + layer.positionTalkChange*apply;
					spriteQuad.transform.rotation = layer.rotation + layer.rotationTalkChange*apply;
					spriteQuad.transform.size = glm::vec3(layer.size + layer.sizeTalkChange*apply, 1);
					layer.curEasingTime++;
				} else if(!talking) layer.curEasingTime=0;
			} else {
				auto& ease=easingsInOut[layer.selectedEasing-1];
				float apply=ease(static_cast<float>(layer.curEasingTime)/layer.easingTimeMs);
				spriteQuad.transform.position = layer.position + layer.positionTalkChange*apply;
				spriteQuad.transform.rotation = layer.rotation + layer.rotationTalkChange*apply;
				spriteQuad.transform.size = glm::vec3(layer.size + layer.sizeTalkChange*apply, 1);
				if(layer.curEasingTime<layer.easingTimeMs && talking) layer.curEasingTime++;
				else if(layer.curEasingTime>0 && !talking) layer.curEasingTime--;
			}
			layer.texture.bind();
			shader->setInt("material.diffuse0", 0);
			shader->setVec3("material.diffuse", glm::vec3(1));
			shader->setMat4("model", spriteQuad.transform.getMatrix());
			
			spriteQuad.model.meshes[0].draw(NULL, true);
		}

		drawUI();
	}
	void techSaveAs() {
		auto r = OS::fileDialog(true, false, "", &viableAvatarFormats, "Select avatar save location");
		if(r.size() > 0) {
			if(r[0]=="") return;
			avatarPath = r[0];
			saveAvatar(avatarPath);
		}
	}
	void techSave() {
		if(avatarPath.empty()||!std::filesystem::exists(avatarPath)) techSaveAs();
		else saveAvatar(avatarPath);
	}
	void techLoad() {
		auto r = OS::fileDialog(false, false, "", &viableAvatarFormats, "Select avatar load location");
		if(r.size() > 0) {
			if(r[0]=="") return;
			avatarPath = r[0];
			loadAvatar(avatarPath);
		}
	}
	void techReload() {
		if(avatarPath.empty() || !std::filesystem::exists(avatarPath)) techLoad();
		else loadAvatar(avatarPath);
	}
	void drawUI() {
		if(hideImGui) return;
		ImGui::Begin("Avatar");
		ImGui::BeginDisabled();
		ImGui::InputText("Path##save_avatar_path", &avatarPath);
		ImGui::EndDisabled();
		if(ImGui::Button("Save (Ctrl+S)##save_avatar")) techSave();
		ImGui::SameLine();
		if(ImGui::Button("Save as... (Ctrl+Shift+S)##save_as_avatar")) techSaveAs();
		if(ImGui::Button("Load (Ctrl+O)##load_avatar")) techLoad();
		ImGui::SameLine();
		if(ImGui::Button("Reload (Ctrl+R)##reload_avatar")) techReload();
		if(ImGui::Button("Options")) configOpen=true;
		if(ImGui::CollapsingHeader("Layers")) {
			for (uint i = 0; i < layers.size(); i++) {
				AvatarLayer& layer = layers[i];
				if(ImGui::MenuItem((layer.name+"##layer_"+std::to_string(i)).c_str(), NULL, selectedSpriteId==i && haveAnyLayerSelected)) {
					selectedSpriteId = i;
					haveAnyLayerSelected = true;
					layerInspectorWindowOpen = true;
				}
			}
			if(ImGui::Button("+ Add layer")) layers.push_back({ true, "Layer " + std::to_string(layers.size()) });
		}
		ImGui::End();
		if(configOpen) {
			ImGui::Begin("Options", &configOpen);
			if(ImGui::BeginPopupModal("Select Microphone")) {
				if(ImGui::Button("Refresh")) inputDevices=AudioIO::getInputDevices();
				ImGui::SameLine();
				if(ImGui::Button("Default")) {
					AudioIO::stop();
					AudioIO::close();
					AudioIO::create();
					AudioIO::start();
					ImGui::CloseCurrentPopup();
					LOGF("Selected device: %s", AudioIO::reciever.name);
				}
				for(uint i=0;i<inputDevices.size();i++) {
					const PaDeviceInfo* device=inputDevices[i];
					if(AudioIO::reciever.name==device->name) continue;
					if(ImGui::MenuItem(Log::formatStr("%s##%i", device->name, i).c_str())) {
						LOGF("Selected device: %s", device->name);
						AudioIO::stop();
						AudioIO::close();
						AudioIO::create(device->name);
						AudioIO::start();
						ImGui::CloseCurrentPopup();
					}
				}
				if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}
			ImGui::Separator(); ImGui::Text("General");
			ImGui::Checkbox("Show all rotation directions", &showAllRotationDirs);
			ImGui::Checkbox("Blinking", &calcBlinking);
			if(ImGui::Checkbox("Transparent background", &transparentWindow)) {
				window.setClearColor({0,0,0,!transparentWindow});
			}
			if(!transparentWindow) {
				glm::vec3 bgc = window.getClearColor();
				if(ImGui::ColorEdit3("Background", &bgc)) window.setClearColor({ bgc, 1 });
			}
			ImGui::Separator(); ImGui::Text("Microphone");
			ImGui::Text(Log::formatStr("%s", AudioIO::reciever.name).c_str());
			ImGui::SameLine();
			if(ImGui::Button((AudioIO::reciever.muted?"U##mute_microphone":"M##mute_microphone"))) {
				AudioIO::reciever.muted=!AudioIO::reciever.muted;
				if(AudioIO::reciever.muted) {
					AudioIO::stop();
					AudioIO::reciever.data.dB=-100;
					AudioIO::reciever.data.rms=0;
					AudioIO::reciever.data.pitch=0;
				} else AudioIO::start();
			}
			ImGui::SameLine();
			if(ImGui::Button("...##select_microphone")) ImGui::OpenPopup("Select Microphone");
			if(AudioIO::reciever.muted) ImGui::Text("Your microphone is muted");
			ImGui::Text("Info");
			ImGui::BeginDisabled();
			ImGui::SliderFloat("Volume (dB)", &AudioIO::reciever.data.dB, -100, 0);
			ImGui::SliderFloat("Pitch", &AudioIO::reciever.data.pitch, 0, 1000);
			ImGui::EndDisabled();
			ImGui::SliderFloat("Cut Off", &AudioIO::reciever.cutOff, -100, 0);
			ImGui::SliderFloat("Amplifier", &AudioIO::reciever.amplifier, 0, 5);
			ImGui::Text("Threshold faker");
			ImGui::Checkbox("Enabled", &fakeThresholdForSmoothTalking);
			if(fakeThresholdForSmoothTalking) ImGui::SliderInt("Delay (ms)", &fakeThresholdForSmoothTalkingMs, 0, 10000);

			ImGui::End();
		}
		if(layerInspectorWindowOpen) {
			ImGui::Begin("Sprite Layer", &layerInspectorWindowOpen);
			if(layers.size() > 0 && selectedSpriteId < layers.size() && haveAnyLayerSelected) {
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
				if(ImGui::TreeNodeEx("Transform", ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::DragFloat3("Position", &layer.position);
					if(showAllRotationDirs) ImGui::DragFloat3("Rotation", &layer.rotation);
					else ImGui::DragFloat("Rotation", &layer.rotation.z);
					ImGui::DragFloat2("Scale", &layer.size);
				}

				ImGui::Separator();
				if(ImGui::TreeNodeEx("Voice alteration", ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
					ImGui::DragFloat3("Position delta##pos_va", &layer.positionTalkChange);
					if(showAllRotationDirs) ImGui::DragFloat3("Rotation delta##rot_va", &layer.rotationTalkChange);
					else ImGui::DragFloat("Rotation delta##rot_va", &layer.rotationTalkChange.z);
					ImGui::DragFloat2("Scale delta##size_va", &layer.sizeTalkChange);
					ImGui::SliderUInt("Easing##ease_voice", &layer.selectedEasing, 0, 9, easingInOutToStr(layer.selectedEasing));
					ImGui::SliderUInt("Easing time", &layer.easingTimeMs, 0, 10000);
					ImGui::Checkbox("Temporal", &layer.temporal);
				}

				ImGui::Separator();
				ImGui::TextColored(ImVec4(1,0,0,1), "!");
				if(ImGui::IsItemHovered()) {
					ImGui::BeginTooltip();
					ImGui::Text("This feature is WIP");
					ImGui::EndTooltip();
				}
				ImGui::SameLine();
				if(ImGui::TreeNodeEx("Idle", ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
					ImGui::Checkbox("Enabled##idle_on", &layer.idleEnabled);
					ImGui::DragFloat3("Position delta##pos_idle", &layer.positionTalkChange);
					if(showAllRotationDirs) ImGui::DragFloat3("Rotation delta##rot_idle", &layer.rotationTalkChange);
					else ImGui::DragFloat("Rotation delta##rot_idle", &layer.rotationTalkChange.z);
					ImGui::DragFloat2("Scale delta##size_idle", &layer.sizeTalkChange);
					ImGui::SliderUInt("Easing##ease_idle", &layer.selectedEasing, 0, 9, easingInOutToStr(layer.selectedEasing));
				}

				ImGui::Separator();
				if(ImGui::TreeNodeEx("Visibility events", ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::SliderEnum<AffectionState>("Show on talking", &layer.showOnTalking, asStrPhysical, AS_UNAFFECTED, AS_ON_FALSE);
					ImGui::SliderEnum<AffectionState>("Show on blinking", &layer.showOnBlinking, asStrPhysical, AS_UNAFFECTED, AS_ON_FALSE);
					if(layer.showOnBlinking!=AS_UNAFFECTED) {
						if(!calcBlinking) ImGui::Text("You have blinking disabled. Changes won't be visible");
						ImGui::Separator(); ImGui::AlignedText(ImGui::TextAligment_Center, "Blinking layer config");
						ImGui::DragUInt("ID", &layer.blinkingLayerId, 1, 0, 15);
						BlinkingLayer& blayer=blinkingLayers[layer.blinkingLayerId];

						bool changed=ImGui::SliderUInt("Cooldown", &blayer.blinkCooldown, 0, 10000);
						changed=ImGui::SliderUInt("Time", &blayer.blinkTime, 0, 10000)||changed;
						changed=ImGui::SliderUInt("Offset", &blayer.offset, 0, 10000)||changed;

						if(changed&&calcBlinking) for(uint i=0;i<blinkingLayers.size();i++) blinkingLayers[i].current=blinkingLayers[i].offset;
					}
				}
			} else {
				ImGui::AlignedText(ImGui::TextAligment_Center, "No Layer selected");
				selectedSpriteId = 0;
				haveAnyLayerSelected = false;
			}
			ImGui::End();
		}
	}
	
	void onShutdown() override {
		AudioIO::remove();
		for(uint i=0;i<layers.size();i++) {
			auto& layer = layers[i];
			layer.texture.destroy();
		}
		layers.clear();
		spriteQuad.destroy();
		shader->destroy();
		saveConfig("config.json");
	}
};

int main() {
	return TubePngApp{}.start("tube.png", 800, 600, WS_NORMAL, WFT_SRGB_TRANSPARENT);
}