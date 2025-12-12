#pragma once
#include <string>
#include "JsonValue.h"

class JsonBool : public JsonValue
{
public:
	JsonBool(bool v) : value(v) {
		this->setType(JsonType::BOOL);
	};
	bool getValue() const { return value; };
private:
	bool value;
};

