#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, shoot;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//hexapod leg to wobble:
	Scene::Transform *player = nullptr;
	struct Enemy {
		glm::vec3 position;
		float sonarCountdown;
		bool sonarActive;
		bool alive;
		std::shared_ptr< Sound::PlayingSample > sonarSound;

		Enemy(glm::vec3 pos) : position(pos), sonarCountdown(0.0f), sonarActive(false), alive(true) { }
	};
	std::vector<Enemy> enemy;

	struct Torpedo {
		glm::vec3 position;
		glm::vec3 direction;
		bool active;
	} torpedo;

	float sonarCounter;
	float attackCounter;

	std::shared_ptr< Sound::PlayingSample > sonarSound;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
