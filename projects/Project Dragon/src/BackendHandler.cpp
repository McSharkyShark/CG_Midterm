#include "BackendHandler.h"
#include "RenderingManager.h"
#include <InputHelpers.h>
#include <IBehaviour.h>
#include <Camera.h>
#include <bullet/LinearMath/btVector3.h>
#include <GreyscaleEffect.h>
#include <PhysicsBody.h>
#define GLM_ENABLE_EXPERIMENTAL
#include<glm/gtx/rotate_vector.hpp>
#include <BtToGlm.h>
#include <ColorCorrection.h>
#include <WorldBuilderV2.h>

GLFWwindow* BackendHandler::window = nullptr;
std::vector<std::function<void()>> BackendHandler::imGuiCallbacks;

void mouse_callback(GLFWwindow* window, double xpos, double ypos);

void BackendHandler::GlDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	{
		std::string sourceTxt;
		switch (source) {
		case GL_DEBUG_SOURCE_API: sourceTxt = "DEBUG"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM: sourceTxt = "WINDOW"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceTxt = "SHADER"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY: sourceTxt = "THIRD PARTY"; break;
		case GL_DEBUG_SOURCE_APPLICATION: sourceTxt = "APP"; break;
		case GL_DEBUG_SOURCE_OTHER: default: sourceTxt = "OTHER"; break;
		}
		switch (severity) {
		case GL_DEBUG_SEVERITY_LOW:          LOG_INFO("[{}] {}", sourceTxt, message); break;
		case GL_DEBUG_SEVERITY_MEDIUM:       LOG_WARN("[{}] {}", sourceTxt, message); break;
		case GL_DEBUG_SEVERITY_HIGH:         LOG_ERROR("[{}] {}", sourceTxt, message); break;
#ifdef LOG_GL_NOTIFICATIONS
		case GL_DEBUG_SEVERITY_NOTIFICATION: LOG_INFO("[{}] {}", sourceTxt, message); break;
#endif
		default: break;
		}
	}
}

bool BackendHandler::InitAll()
{
	Logger::Init();
	Util::Init();

	if (!InitGLFW())
		return 1;
	if (!InitGLAD())
		return 1;
	Framebuffer::InitFullscreenQuad();
	RenderingManager::Init();

	InitImGui();
}

void BackendHandler::GlfwWindowResizedCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	RenderingManager::activeScene->Registry().view<Camera>().each([=](Camera& cam)
		{
			cam.ResizeWindow(width, height);
		});
	RenderingManager::activeScene->Registry().view<Framebuffer>().each([=](Framebuffer& buf)
		{
			buf.Reshape(width, height);
		});
	RenderingManager::activeScene->Registry().view<PostEffect>().each([=](PostEffect& buf)
		{
			buf.Reshape(width, height);
		});
	RenderingManager::activeScene->Registry().view<GreyscaleEffect>().each([=](GreyscaleEffect& buf)
		{
			buf.Reshape(width, height);
		});
	RenderingManager::activeScene->Registry().view<ColorCorrectionEffect>().each([=](ColorCorrectionEffect& buf)
		{
			buf.Reshape(width, height);
		});
}

#include <Player.h>
#include <Enemy.h>
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		RenderingManager::activeScene->FindFirst("Camera").get<Player>().FireWeapon(0);
			
	}
}
bool texToggle = 1;

void BackendHandler::UpdateInput()
{
	//creates a single camera object to call

	GameObject cameraObj = RenderingManager::activeScene->FindFirst("Camera");
	//loads the LUTS to switch them

	Camera cam = cameraObj.get<Camera>();
	Transform t = cameraObj.get<Transform>();
	PhysicsBody phys = cameraObj.get<PhysicsBody>();
	//get a forward vector using fancy maths
	glm::vec3 forward(0, 1, 0);
	forward = glm::rotate(forward, glm::radians(t.GetLocalRotation().z), glm::vec3(0, 0, 1));
	glm::normalize(forward);

	btVector3 movement = btVector3(0, 0, 0);
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		movement += BtToGlm::GLMTOBTV3(forward *1.8f);
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		movement -= BtToGlm::GLMTOBTV3(forward *1.8f);
	}

	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		glm::vec3 direction = glm::normalize(glm::cross(forward, cam.GetUp()));
		movement.setX(movement.getX() - direction.x * 1.8);
		movement.setY(movement.getY() - direction.y * 1.8);
	}

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		glm::vec3 direction = -glm::normalize(glm::cross(forward, cam.GetUp()));
		movement.setX(movement.getX() - direction.x * 1.8);
		movement.setY(movement.getY() - direction.y * 1.8);
	}

	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
	{
		movement.setZ(1.0f);;
	}
	if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)
	{
		WorldBuilderV2 build;
		build.BuildNewWorld();
	}

	phys.ApplyForce(movement/2);
	
	if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS)
	{
		RenderingManager::NoOutline->SetUniform("u_Texturetoggle", (int)texToggle);
	}
	if (glfwGetKey(window, GLFW_KEY_6) == GLFW_RELEASE)
	{
		texToggle != texToggle;
	}

	RenderingManager::activeScene->Registry().view<BehaviourBinding>().each([&](entt::entity entity, BehaviourBinding& binding) {
		// Iterate over all the behaviour scripts attached to the entity, and update them in sequence (if enabled)
		for (const auto& behaviour : binding.Behaviours) {
			if (behaviour->Enabled) {
				behaviour->Update(entt::handle(RenderingManager::activeScene->Registry(), entity));
			}
		}
		}
	);
}

bool BackendHandler::InitGLFW()
{
	if (glfwInit() == GLFW_FALSE) {
		LOG_ERROR("Failed to initialize GLFW");
		return false;
	}

#ifdef _DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif

	//Create a new GLFW window
	window = glfwCreateWindow(1280, 720, "Project Dragon", nullptr, nullptr);
	glfwMakeContextCurrent(window);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	// Set our window resized callback
	glfwSetWindowSizeCallback(window, GlfwWindowResizedCallback);

	glfwSetMouseButtonCallback(window, mouse_button_callback);

	// Store the window in the application singleton
	Application::Instance().Window = window;

	return true;
}

bool BackendHandler::InitGLAD()
{
	if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) {
		LOG_ERROR("Failed to initialize Glad");
		return false;
	}
	return true;
}

void BackendHandler::InitImGui()
{
	// Creates a new ImGUI context
	ImGui::CreateContext();
	// Gets our ImGUI input/output
	ImGuiIO& io = ImGui::GetIO();
	// Enable keyboard navigation
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	// Allow docking to our window
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// Allow multiple viewports (so we can drag ImGui off our window)
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	// Allow our viewports to use transparent backbuffers
	io.ConfigFlags |= ImGuiConfigFlags_TransparentBackbuffers;

	// Set up the ImGui implementation for OpenGL
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 410");

	// Dark mode FTW
	ImGui::StyleColorsDark();

	// Get our imgui style
	ImGuiStyle& style = ImGui::GetStyle();
	//style.Alpha = 1.0f;
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 0.8f;
	}
}

void BackendHandler::ShutdownImGui()
{
	// Cleanup the ImGui implementation
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	// Destroy our ImGui context
	ImGui::DestroyContext();
}

void BackendHandler::RenderImGui()
{
	// Implementation new frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	// ImGui context new frame
	ImGui::NewFrame();

	if (ImGui::Begin("Debug")) {
		// Render our GUI stuff
		for (auto& func : imGuiCallbacks) {
			func();
		}
		ImGui::End();
	}

	// Make sure ImGui knows how big our window is
	ImGuiIO& io = ImGui::GetIO();
	int width{ 0 }, height{ 0 };
	glfwGetWindowSize(window, &width, &height);
	io.DisplaySize = ImVec2((float)width, (float)height);

	// Render all of our ImGui elements
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// If we have multiple viewports enabled (can drag into a new window)
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		// Update the windows that ImGui is using
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		// Restore our gl context
		glfwMakeContextCurrent(window);
	}
}

void BackendHandler::RenderVAO(const Shader::sptr& shader, const VertexArrayObject::sptr& vao, const glm::mat4& viewProjection, const Transform& transform)
{
	shader->SetUniformMatrix("u_ModelViewProjection", viewProjection * transform.WorldTransform());
	shader->SetUniformMatrix("u_Model", transform.WorldTransform());
	shader->SetUniformMatrix("u_NormalMatrix", transform.WorldNormalMatrix());
	vao->Render();
}

void BackendHandler::SetupShaderForFrame(const Shader::sptr& shader, const glm::mat4& view, const glm::mat4& projection)
{
	shader->Bind();
	// These are the uniforms that update only once per frame
	shader->SetUniformMatrix("u_View", view);
	shader->SetUniformMatrix("u_ViewProjection", projection * view);
	shader->SetUniformMatrix("u_SkyboxMatrix", projection * glm::mat4(glm::mat3(view)));
	glm::vec3 camPos = glm::inverse(view) * glm::vec4(0, 0, 0, 1);
	shader->SetUniform("u_CamPos", camPos);
}

//for camera forward
bool firstMouse = true;
float yaw = -90.0f;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
float pitch = 0.0f;
float lastX = 1280 / 2.0;
float lastY = 720.0 / 2.0;