#pragma once
#include "JsonValue.h"
#include <memory>
#include <vector>

class JsonArray : public JsonValue
{
public:
	JsonArray() {
		this->setType(JsonType::ARRAY);
	};

	void add(std::unique_ptr<JsonValue> value) {
		members.push_back(std::move(value));
	};

	int size() const { return members.size(); };

	JsonValue& operator[] (int index) {
		return *members[index];
	};

	const JsonValue& operator[] (int index) const {
		return *members[index];
	};

	auto begin() { return members.begin(); }
	auto end() { return members.end(); }
	auto begin() const { return members.begin(); }
	auto end()   const { return members.end(); }
private:
	std::vector<std::unique_ptr<JsonValue>> members;
};

