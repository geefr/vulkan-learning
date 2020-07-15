
#include "engine.h"

#include "meshnode.h"
#include "gltfloader.h"

#include <exception>
#include <iostream>

int main(int argc, char* argv[])
{
  std::string modelFile;
  if( argc > 1 ) {
      modelFile = argv[1];
  }

  try {
    Engine eng;

    // Setup global actions
    eng.addGlobalEventCallback([](Engine& engine, Event& e) {
      if( auto keypress = dynamic_cast<KeyPressEvent*>(&e) ) {
        if( keypress->mKey == 256 ) engine.quit();
      }
    });

    // Load some data into the scene
    if( !modelFile.empty() ) {
        auto modelNode = GLTFLoader::load(modelFile);
        if( !modelNode ) {
            std::cerr << "ERROR: Failed to load model: " << modelFile << std::endl;
            return EXIT_FAILURE;
        }
        modelNode->updateScript([](Engine&, Node& n, double deltaT) {
           n.rotation() = n.rotation() + glm::vec3(0.0, deltaT * 1.0, 0.0);
        });
        eng.nodegraph()->children().emplace_back(modelNode);
    } else {
        std::shared_ptr<MeshNode> dummyMesh(new MeshNode());
        dummyMesh->updateScript([](Engine&, Node& n, double deltaT){
            n.rotation() = n.rotation() + glm::vec3(0.0, 0.0, deltaT * 1.0);
        });
        eng.nodegraph()->children().emplace_back(dummyMesh);
    }

    eng.run();
  } catch ( std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
