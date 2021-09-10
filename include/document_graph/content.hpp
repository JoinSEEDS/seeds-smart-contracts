#pragma once

#include <variant>

#include <eosio/name.hpp>
#include <eosio/asset.hpp>
#include <eosio/time.hpp>
#include <eosio/crypto.hpp>

namespace hypha
{
    struct Content
    {
        typedef std::variant<std::monostate, eosio::name, std::string, eosio::asset, eosio::time_point,
                             std::int64_t, eosio::checksum256>
            FlexValue;

    public:
        Content();
        Content(std::string label, FlexValue value);
        ~Content();

        const bool isEmpty() const;

        const std::string toString() const;

        // NOTE: not using m_ notation because this changes serialization format
        std::string label;
        FlexValue value;

        //Can return reference to stored type
        template <class T>
        inline decltype(auto) getAs()
        {
            eosio::check(std::holds_alternative<T>(value), "Content value is not of expected type");
            return std::get<T>(value);
        }

        template <class T>
        inline decltype(auto) getAs() const
        {
            eosio::check(std::holds_alternative<T>(value), "Content value is not of expected type");
            return std::get<T>(value);
        }

        inline bool operator==(const Content& other) 
        {
          return label == other.label && value == other.value;
        }

        EOSLIB_SERIALIZE(Content, (label)(value))
    };

} // namespace hypha