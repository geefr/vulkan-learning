
#include "engine.h"

#include "meshnode.h"
#include "loaders/gltfloader.h"

#include <exception>
#include <iostream>

int main(int argc, char* argv[])
{
  std::string modelFile;
  if( argc > 1 ) {
      modelFile = argv[1];
  }

  try {
    // Create the engine, window, renderer, etc.
    Engine eng;

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

    static bool camActive = true;

    // Event Handling
    eng.addGlobalEventCallback([](Engine& engine, Event& e) {
      if( auto keypress = dynamic_cast<KeyPressEvent*>(&e) ) {
        if( keypress->mKey == 256 ) engine.quit();
        else if( keypress->mKey == ' ') camActive = !camActive;
      }
    });


    // A global per-update call (TODO: Just hooking the root node here, may be better as a bunch of callback on the engine, independent of the node graph?)
    eng.nodegraph()->updateScript([](Engine& eng, Node&, double deltaT) {
        static float camRot = 0.0; // TODO: Should have a way to store attributes on the engine/nodes? Avoid a static here?
        static bool increaseRot = false;

        // Just sweep the camera around the scene for now
        if( camActive ) {
          if( increaseRot ) {
              camRot += 1.5 * deltaT;
              if( camRot > glm::radians(45.f)) increaseRot = false;
          } else {
              camRot -= 1.5 * deltaT;
              if( camRot < glm::radians(-45.f)) increaseRot = true;
          }
         }

        glm::vec4 camPos(0.0, 2.0, -5.0, 1.0);
        glm::mat4 camRotMat(1.0f);
        camRotMat = glm::rotate(camRotMat, camRot, glm::vec3(0.0, 1.0, 0.0));
        camPos = camRotMat * camPos;
        eng.camera().lookAt(camPos, glm::vec3(0.0,0.0,0.0), glm::vec3(0.0,1.0,0.0));
    });

    // Start the engine!
    eng.run();

  } catch ( std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
