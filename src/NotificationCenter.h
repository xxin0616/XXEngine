#pragma once
#ifndef _XXENGINE_NOTIFICATION_CENTER_H_
#define _XXENGINE_NOTIFICATION_CENTER_H_

#include <string>

namespace NotificationCenter
{
	void Push(const std::string& message, float duration_seconds = 3.0f);
	void SetPersistent(const std::string& key, const std::string& message);
	void ClearPersistent(const std::string& key);
	void Render();
}

#endif // !_XXENGINE_NOTIFICATION_CENTER_H_
