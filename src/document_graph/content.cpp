#include <document_graph/content.hpp>
#include <document_graph/util.hpp>

namespace hypha
{

    Content::Content(std::string label, FlexValue value) : label{label}, value{value} {}
    Content::Content() {}
    Content::~Content() {}

    const bool Content::isEmpty () const
    {
        if (std::holds_alternative<std::monostate>(value)) {
            return true;
        }
        return false;
    }

    const std::string Content::toString() const
    {
        if (isEmpty()) return "";
        
        std::string str = "{" + std::string(label) + "=";
        if (std::holds_alternative<std::int64_t>(value))
        {
            str += "[int64," + std::to_string(std::get<std::int64_t>(value)) + "]";
        }
        else if (std::holds_alternative<eosio::asset>(value))
        {
            str += "[asset," + std::get<eosio::asset>(value).to_string() + "]";
        }
        else if (std::holds_alternative<eosio::time_point>(value))
        {
            str += "[time_point," + std::to_string(std::get<eosio::time_point>(value).sec_since_epoch()) + "]";
        }
        else if (std::holds_alternative<std::string>(value))
        {
            str += "[string," + std::get<std::string>(value) + "]";
        }
        else if (std::holds_alternative<eosio::checksum256>(value))
        {
            eosio::checksum256 cs_value = std::get<eosio::checksum256>(value);
            auto arr = cs_value.extract_as_byte_array();
            std::string str_value = toHex((const char *)arr.data(), arr.size());
            str += "[checksum256," + str_value + "]";
        }
        else
        {
            str += "[name," + std::get<eosio::name>(value).to_string() + "]";
        }
        str += "}";
        return str;
    }
} // namespace hypha