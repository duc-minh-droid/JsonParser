#pragma once
#include <string>
#include "JsonValue.h"

class JsonString : public JsonValue
{
public:
	JsonString(std::string_view v = "") : value(v) {
		this->setType(JsonType::STRING);
	};
	std::string getValue() const { return value; };
	char& operator[](size_t i) {
		return value[i];
	}
private:
	std::string value;
};

