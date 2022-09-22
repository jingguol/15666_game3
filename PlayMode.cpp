#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint submarine_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > submarine_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("submarine.pnct"));
	submarine_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > hexapod_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("submarine.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = submarine_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = submarine_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > sonar1_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sonar1.opus"));
});

Load< Sound::Sample > sonar2_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sonar2.opus"));
});

Load< Sound::Sample > shoot_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("shoot.opus"));
});

Load< Sound::Sample > explosion_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("explosion.opus"));
});


PlayMode::PlayMode() : scene(*hexapod_scene) {
	for (auto &transform : scene.transforms) {
		if (transform.name == "submarine") this->player = &transform;
	}
	if (this->player == nullptr) throw std::runtime_error("submarine not found.");

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	sonarCounter = 5.0f;
	attackCounter = 0.0f;

	for (int i = 0; i < 10; ++i) {
		float x = static_cast<float>(rand() % 200 - 100);
		float y = static_cast<float>(rand() % 200 - 100);
		enemy.push_back(Enemy(glm::vec3(x, y, 0.0f)));
	}
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_LEFT) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_UP) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_z) {
			shoot.downs += 1;
			shoot.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_LEFT) {
			left.pressed = false;
			return true;
		} if (evt.key.keysym.sym == SDLK_UP) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			right.pressed = false;
			return true;
		}
	} 

	return false;
}

void PlayMode::update(float elapsed) {

	// move and rotate
	{
		//combine inputs into a move:
		constexpr float playerSpeed = 10.0f;
		
		if (!left.pressed && right.pressed) {
			camera->transform->rotation = camera->transform->rotation * 
				glm::angleAxis(-1.0f * elapsed, glm::vec3(0.0f, 0.0f, 1.0f));
			player->rotation = player->rotation * 
				glm::angleAxis(-1.0f * elapsed, glm::vec3(0.0f, 0.0f, 1.0f));
		}
		if (!right.pressed && left.pressed) {
			camera->transform->rotation = camera->transform->rotation * 
				glm::angleAxis(1.0f * elapsed, glm::vec3(0.0f, 0.0f, 1.0f));
			player->rotation = player->rotation * 
				glm::angleAxis(1.0f * elapsed, glm::vec3(0.0f, 0.0f, 1.0f));
		}
		if (!down.pressed && up.pressed) {
			glm::mat4x3 frame = camera->transform->make_local_to_parent();
			// glm::vec3 frame_right = frame[0];
			glm::vec3 frame_up = frame[1];
			// glm::vec3 frame_forward = -frame[2];
			camera->transform->position += playerSpeed * frame_up * elapsed;
			player->position = camera->transform->position;
			player->position.z = 0.0f;
		}
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	sonarCounter -= elapsed;
	if (sonarCounter <= 0) {
		sonarSound = Sound::play_3D(*sonar1_sample, 1.0f, player->position);
		sonarCounter = 5.0f;

		for (auto it = enemy.begin(); it != enemy.end(); ++it) {
			if (!it->sonarActive) {
				float distance = glm::distance(it->position, player->position);
				if (it->alive && distance <= 30.0f) {
					it->sonarActive = true;
					it->sonarCountdown = (distance / 30.0f) * 4.0f;
				}
			}
		}
	}

	for (auto it = enemy.begin(); it != enemy.end(); ++it) {
		if (it->sonarActive) {
			it->sonarCountdown -= elapsed;
			if (it->sonarCountdown <= 0.0f) {
				float distance = glm::distance(it->position, player->position);
				it->sonarSound = Sound::play_3D(*sonar2_sample, distance / 30.0f, it->position);
				it->sonarActive = false;
			}
		}
	}

	if (shoot.downs > 0) {
		if (attackCounter <= 0.0f) {
			Sound::play_3D(*shoot_sample, 1.0f, player->position);
			attackCounter = 10.0f;
			torpedo.active = true;
			torpedo.position = player->position;
			glm::mat4x3 frame = camera->transform->make_local_to_parent();
			glm::vec3 frame_up = frame[1];
			torpedo.direction = frame_up;
		}
	}

	if (attackCounter > 0.0f) {
		attackCounter -= elapsed;
	}

	constexpr float torpedoSpeed = 25.0f;
	if (torpedo.active) {
		torpedo.position += torpedoSpeed * elapsed * torpedo.direction;
		for (auto it = enemy.begin(); it != enemy.end(); ++it) {
			if (glm::distance(torpedo.position, it->position) <= 5.0f) {
				it->alive = false;
				torpedo.active = false;
				Sound::play_3D(*explosion_sample, 1.0f, it->position);
			}
		}
		if (torpedo.position.x < -100.0f || torpedo.position.x > 100.0f ||
			torpedo.position.y < -100.0f || torpedo.position.y > 100.0f) {
				torpedo.active = false;
			}
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
	shoot.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	GL_ERRORS();
}

