#include <firesteel/firesteel.hpp>
using namespace Firesteel;

// Entities are basicly 3d models,
// that will be rendered in update function.
Entity box{glm::vec3(0, 0, -5), glm::vec3(45, 45, 0)};
// Shader that will define how the box is drawn.
// Learn more: https://learnopengl.com/Getting-started/Shaders
std::shared_ptr<Shader> shader;
// Camera that will help with displaying box in perspective.
Camera camera{glm::vec3(0), glm::vec3(0, 0, -90)};

class ExampleApp : public App {
	// Runs after window and renderer initialization.
	void onInitialize() override {
		// Loads model for entity from file.
		box.load("res\\box.obj");
		// Initializes shader from vertex and fragment ones.
		shader=std::make_shared<Shader>("res/shader.vs", "res/shader.fs");
		box.setMaterialsShader(shader);
		// Get current camera vectors;
		camera.update();
	}
	
	// Runs each frame.
	void onUpdate() override {
		// Update camera aspect.
		camera.aspect = window.aspect();
		// Store camera's view and projection.
		// Learn more: https://learnopengl.com/Getting-started/Camera
		glm::mat4 projection = camera.getProjection(),
			view = camera.getView();
		// Enables and sets parameters of shader for box.
		shader->enable();
		shader->setMat4("projection", projection);
		shader->setMat4("view", view);
		// Draw the box itself.
		box.draw();
	}
	
	// Runs after window.close() is called or on window closing.
	void onShutdown() override {
		// Clean up leftover resources.
		box.remove();
		shader->remove();
	}
};

int main() {
	// Initializes your application.
	return ExampleApp{}.start();
}