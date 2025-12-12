#pragma once
#include "JsonValue.h"
#include <unordered_map>
#include <memory>

class JsonObject : public JsonValue
{
public:
	JsonObject() {
		this->setType(JsonType::OBJECT);
	};

	void add(std::string key, std::unique_ptr<JsonValue> value) {
		members[key] = std::move(value);
	};
	JsonValue& get(std::string key) { 
		return *members.at(key); 
	};
	bool has(std::string key) {
		return members.count(key) == 0 ? false : true;
	};
	void remove(std::string key) {
		members.erase(key);
	}

	int size() const { return members.size(); };

	auto begin() { return members.begin(); }
	auto end() { return members.end(); }
	auto begin() const { return members.begin(); }
	auto end()   const { return members.end(); }
private:
	std::unordered_map<std::string, std::unique_ptr<JsonValue>> members;
};

