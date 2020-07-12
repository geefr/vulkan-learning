#include "event.h"

Event::Event() {}
Event::~Event() {}
	
KeyPressEvent::KeyPressEvent(int key)
	: mKey(key)
{}

KeyPressEvent::~KeyPressEvent() {}

KeyReleaseEvent::KeyReleaseEvent(int key)
	: mKey(key)
{}

KeyReleaseEvent::~KeyReleaseEvent() {}

