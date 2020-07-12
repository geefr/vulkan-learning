/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#ifndef EVENT_H
#define EVENT_H

#include <functional>

class Event {
public:
	virtual ~Event();

protected:
	Event();
};

class KeyPressEvent : public Event {
public:
	KeyPressEvent(int key);
	virtual ~KeyPressEvent();

	int mKey;
};

class KeyReleaseEvent : public Event {
public:
	KeyReleaseEvent(int key);
	virtual ~KeyReleaseEvent();

	int mKey;
};
#endif
