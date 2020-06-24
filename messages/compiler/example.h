/***************************************
 Autogenerated by cmfc. Do not modify.
***************************************/

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "serialize.h"

namespace concord::messages {

struct NewViewElement {
  static constexpr uint32_t id = 1; 

  uint16_t replica_id; 
  std::vector<uint8_t> digest; 
};

void Serialize(std::vector<uint8_t>& output, const NewViewElement& t) {
  cmf::Serialize(output, t.replica_id);
  cmf::Serialize(output, t.digest);
}

void Deserialize(std::vector<uint8_t>::const_iterator& input, NewViewElement& t) {
  cmf::Deserialize(input, t.replica_id);
  cmf::Deserialize(input, t.digest);
}

struct Transaction {
  static constexpr uint32_t id = 2; 

  std::string name; 
  std::vector<std::pair<std::string, std::string>> actions; 
  std::optional<std::vector<uint8_t>> auth_key; 
};

void Serialize(std::vector<uint8_t>& output, const Transaction& t) {
  cmf::Serialize(output, t.name);
  cmf::Serialize(output, t.actions);
  cmf::Serialize(output, t.auth_key);
}

void Deserialize(std::vector<uint8_t>::const_iterator& input, Transaction& t) {
  cmf::Deserialize(input, t.name);
  cmf::Deserialize(input, t.actions);
  cmf::Deserialize(input, t.auth_key);
}

struct Envelope {
  static constexpr uint32_t id = 3; 

  uint32_t version; 
  std::variant<Transaction, NewViewElement> x; 
};

void Serialize(std::vector<uint8_t>& output, const std::variant<Transaction, NewViewElement>& val) {
  std::visit([&output](auto&& arg){
    cmf::Serialize(output, arg.id);
    Serialize(output, arg);
  }, val);
}
void Serialize(std::vector<uint8_t>& output, const Envelope& t) {
  cmf::Serialize(output, t.version);
  Serialize(output, t.x);
}

void Deserialize(std::vector<uint8_t>::const_iterator& start, std::variant<Transaction, NewViewElement>& val) {
  uint32_t id;
  cmf::Deserialize(start, id);

  if (id == 2) {
    Transaction value;
    Deserialize(start, value);
    val = value;
    return;
  }

  throw cmf::DeserializeError(std::string("Invalid Message id in variant: ") + std::to_string(id));
}

  if (id == 1) {
    NewViewElement value;
    Deserialize(start, value);
    val = value;
    return;
  }

  throw cmf::DeserializeError(std::string("Invalid Message id in variant: ") + std::to_string(id));
}

void Deserialize(std::vector<uint8_t>::const_iterator& input, Envelope& t) {
  cmf::Deserialize(input, t.version);
  Deserialize(input, t.x);
}

struct NewStuff {
  static constexpr uint32_t id = 4; 

  std::map<std::string, std::vector<std::pair<std::string, std::string>>> crazy_map; 
};

void Serialize(std::vector<uint8_t>& output, const NewStuff& t) {
  cmf::Serialize(output, t.crazy_map);
}

void Deserialize(std::vector<uint8_t>::const_iterator& input, NewStuff& t) {
  cmf::Deserialize(input, t.crazy_map);
}

struct WithMsgRefs {
  static constexpr uint32_t id = 5; 

  NewStuff new_stuff; 
  std::vector<Transaction> tx_list; 
  std::map<std::string, Envelope> map_of_envelope; 
};

void Serialize(std::vector<uint8_t>& output, const WithMsgRefs& t) {
  Serialize(output, t.new_stuff);
  cmf::Serialize(output, t.tx_list);
  cmf::Serialize(output, t.map_of_envelope);
}

void Deserialize(std::vector<uint8_t>::const_iterator& input, WithMsgRefs& t) {
  Deserialize(input, t.new_stuff);
  cmf::Deserialize(input, t.tx_list);
  cmf::Deserialize(input, t.map_of_envelope);
}

} // namespace concord::messages
