#pragma once
#include <string>
#include "JsonValue.h"

class JsonNumber: public JsonValue
{
public:
	JsonNumber(float v): value(v) {
		this->setType(JsonType::NUMBER);
	};
	float getValue() const { return value; };
private:
	float value;
};

