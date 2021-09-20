#pragma once

#include <string>
#include <eosio/crypto.hpp>
#include <eosio/name.hpp>

namespace hypha
{

    const std::string toHex(const char *d, std::uint32_t s);
    const std::string readableHash(const eosio::checksum256 &hash);
    const std::uint64_t toUint64(const std::string &fingerprint);
    const std::uint64_t concatHash(const eosio::checksum256 sha1, const eosio::checksum256 sha2, const eosio::name label);
    const std::uint64_t concatHash(const eosio::checksum256 sha1, const eosio::checksum256 sha2);
    const std::uint64_t concatHash(const eosio::checksum256 sha, const eosio::name label);

} // namespace hypha