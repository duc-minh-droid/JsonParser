#pragma once
#include <string>
#include "JsonValue.h"

class JsonNumber: public JsonValue
{
public:
	JsonNumber(double v): value(v) {
		this->setType(JsonType::NUMBER);
	};
	double getValue() const { return value; };
private:
	double value;
};
