
#include "registrar.h"
#include "util.h"

#include <iostream>
#include <fstream>

Registrar Registrar::mReg;

Registrar::Registrar()
{
}

Registrar::~Registrar()
{

}

void Registrar::tearDown() {

  // TODO: Not sure if all this is needed?
  // Initially thought the smart pointers would
  // be best but might be hard to sort out
  // cleanup order in that case..
  mDeviceInstance->waitAllDevicesIdle();

  mFrameBuffer.release();
  mGraphicsPipeline.release();

  mWindowIntegration.release();

  Util::release();

  mDeviceInstance.release();
}

