
#include "engine.h"

#include "meshnode.h"

#include <exception>
#include <iostream>

int main(int argc, char* argv[])
{
  try {
    Engine eng;

    // Setup global actions
    eng.addGlobalEventCallback([](Engine& engine, Event& e) {
      if( auto keypress = dynamic_cast<KeyPressEvent*>(&e) ) {
        if( keypress->mKey == 256 ) engine.quit();
      }
    });

    // Load some data into the scene
    std::shared_ptr<MeshNode> triangle(new MeshNode());
    triangle->updateScript([](Engine&, Node& n, double deltaT){
		n.rotation() = n.rotation() + glm::vec3(0.0, 0.0, deltaT * 1.0);
	});
    eng.nodegraph()->children().emplace_back(triangle);

    eng.run();
  } catch ( std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
