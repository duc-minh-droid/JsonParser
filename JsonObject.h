#pragma once
#include "JsonValue.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Members are kept in insertion order (like most JSON libraries print them).
// Lookup is linear, which is fine for the object sizes this parser targets.
class JsonObject : public JsonValue
{
public:
	using Member = std::pair<std::string, std::unique_ptr<JsonValue>>;

	JsonObject() {
		this->setType(JsonType::OBJECT);
	};

	// Adding an existing key replaces its value (last duplicate wins).
	void add(std::string key, std::unique_ptr<JsonValue> value) {
		for (auto& m : members) {
			if (m.first == key) { m.second = std::move(value); return; }
		}
		members.emplace_back(std::move(key), std::move(value));
	};
	JsonValue& get(const std::string& key) {
		for (auto& m : members) {
			if (m.first == key) return *m.second;
		}
		throw std::out_of_range("JsonObject: no key \"" + key + "\"");
	};
	bool has(const std::string& key) const {
		for (const auto& m : members) {
			if (m.first == key) return true;
		}
		return false;
	};
	void remove(const std::string& key) {
		for (auto it = members.begin(); it != members.end(); ++it) {
			if (it->first == key) { members.erase(it); return; }
		}
	}

	int size() const { return static_cast<int>(members.size()); };

	auto begin() { return members.begin(); }
	auto end() { return members.end(); }
	auto begin() const { return members.begin(); }
	auto end()   const { return members.end(); }
private:
	std::vector<Member> members;
};
