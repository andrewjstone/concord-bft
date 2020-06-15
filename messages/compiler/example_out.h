
/***************************************
 Autogenerated by cmfc. Do not modify.
***************************************/

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>


struct Transaction {
  static constexpr uint32_t id = 2; 

  std::string name; 
  std::vector<std::pair<std::string, std::vector<uint8_t>>> actions; 
  std::optional<std::vector<uint8_t>> auth_key; 
};

struct NewViewElement {
  static constexpr uint32_t id = 1; 

  uint16_t replica_id; 
  std::vector<uint8_t> digest; 
};

struct Envelope {
  static constexpr uint32_t id = 3; 

  uint32_t version; 
  std::variant<Transaction, NewViewElement> None; 
};