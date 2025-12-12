#pragma once
#include <string>
#include "JsonValue.h"

class JsonNull : public JsonValue
{
public:
	JsonNull() {
		this->setType(JsonType::NULLL);
	};
	bool isNull() const { return true; };
};

