#include <eosio/crypto.hpp>
#include <eosio/name.hpp>

#include <document_graph/util.hpp>

namespace hypha
{

    const std::string toHex(const char *d, std::uint32_t s)
    {
        std::string r;
        const char *to_hex = "0123456789abcdef";
        auto c = reinterpret_cast<const uint8_t *>(d);
        for (auto i = 0; i < s; ++i)
            (r += to_hex[(c[i] >> 4)]) += to_hex[(c[i] & 0x0f)];
        return r;
    }

    const std::string readableHash(const eosio::checksum256 &hash)
    {
        auto byte_arr = hash.extract_as_byte_array();
        return toHex((const char *)byte_arr.data(), byte_arr.size());
    }

    const std::uint64_t toUint64(const std::string &fingerprint)
    {
        uint64_t id = 0;
        eosio::checksum256 h = eosio::sha256(const_cast<char *>(fingerprint.c_str()), fingerprint.size());
        auto hbytes = h.extract_as_byte_array();
        for (int i = 0; i < 4; i++)
        {
            id <<= 8;
            id |= hbytes[i];
        }
        return id;
    }

    const uint64_t concatHash(const eosio::checksum256 sha1, const eosio::checksum256 sha2, const eosio::name label)
    {
        return toUint64(readableHash(sha1) + readableHash(sha2) + label.to_string());
    }

    const uint64_t concatHash(const eosio::checksum256 sha1, const eosio::checksum256 sha2)
    {
        return toUint64(readableHash(sha1) + readableHash(sha2));
    }

    const uint64_t concatHash(const eosio::checksum256 sha, const eosio::name label)
    {
        return toUint64(readableHash(sha) + label.to_string());
    }

} // namespace hypha