#pragma once
#include <string>

enum JsonType {
	NUMBER,
	STRING,
	BOOL,
	OBJECT,
	ARRAY,
	NULLL
};

class JsonValue
{
public:
	JsonValue() {};
	virtual ~JsonValue() = default;

	void setType(JsonType t) { type = t; }
	JsonType getType() const { return type; }
private: 
	JsonType type{};
};

