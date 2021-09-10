#pragma once

#include <eosio/name.hpp>
#include <eosio/time.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/crypto.hpp>

namespace hypha
{
    struct Edge
    {
        Edge();
        Edge(const eosio::name &contract, const eosio::name &creator, const eosio::checksum256 &fromNode,
             const eosio::checksum256 &toNode, const eosio::name &edgeName);
        ~Edge();

        void emplace();
        void erase();

        const eosio::checksum256 &getFromNode() { return from_node; }
        const eosio::checksum256 &getToNode() { return to_node; }
        const eosio::name &getEdgeName() { return edge_name; }
        const eosio::time_point &getCreated() { return created_date; }
        const eosio::name &getCreator() { return creator; }
        const eosio::name &getContract() { return contract; }

        static Edge getOrNew(const eosio::name &contract,
                             const eosio::name &creator,
                             const eosio::checksum256 &from_node,
                             const eosio::checksum256 &to_node,
                             const eosio::name &edge_name);

        static void write(const eosio::name &_contract,
                          const eosio::name &_creator,
                          const eosio::checksum256 &_from_node,
                          const eosio::checksum256 &_to_node,
                          const eosio::name &_edge_name);

        static Edge get(const eosio::name &contract,
                        const eosio::checksum256 &from_node,
                        const eosio::checksum256 &to_node,
                        const eosio::name &edge_name);

        static std::pair<bool, Edge> getIfExists(const eosio::name &_contract,
                                                 const eosio::checksum256 &_from_node,
                                                 const eosio::name &_edge_name);

        static Edge get(const eosio::name &contract,
                        const eosio::checksum256 &from_node,
                        const eosio::name &edge_name);

        static std::vector<Edge> getAll(const eosio::name &_contract,
                           const eosio::checksum256 &_from_node,
                           const eosio::name &_edge_name);

        static bool exists(const eosio::name &_contract,
                           const eosio::checksum256 &_from_node,
                           const eosio::checksum256 &_to_node,
                           const eosio::name &_edge_name);

        uint64_t id; // hash of from_node, to_node, and edge_name

        // these three additional indexes allow isolating/querying edges more precisely (less iteration)
        uint64_t from_node_edge_name_index;
        uint64_t from_node_to_node_index;
        uint64_t to_node_edge_name_index;

        // these members should be private, but they are used in DocumentGraph for edge replacement logic
        eosio::checksum256 from_node;
        eosio::checksum256 to_node;
        eosio::name edge_name;
        eosio::time_point created_date;
        eosio::name creator;
        eosio::name contract;

        uint64_t primary_key() const;
        uint64_t by_from_node_edge_name_index() const;
        uint64_t by_from_node_to_node_index() const;
        uint64_t by_to_node_edge_name_index() const;
        uint64_t by_edge_name() const;
        uint64_t by_created() const;
        uint64_t by_creator() const;
        eosio::checksum256 by_from() const;
        eosio::checksum256 by_to() const;

        EOSLIB_SERIALIZE(Edge, (id)(from_node_edge_name_index)(from_node_to_node_index)(to_node_edge_name_index)(from_node)(to_node)(edge_name)(created_date)(creator)(contract))

        typedef eosio::multi_index<eosio::name("edges"), Edge,
                                   eosio::indexed_by<eosio::name("fromnode"), eosio::const_mem_fun<Edge, eosio::checksum256, &Edge::by_from>>,
                                   eosio::indexed_by<eosio::name("tonode"), eosio::const_mem_fun<Edge, eosio::checksum256, &Edge::by_to>>,
                                   eosio::indexed_by<eosio::name("edgename"), eosio::const_mem_fun<Edge, uint64_t, &Edge::by_edge_name>>,
                                   eosio::indexed_by<eosio::name("byfromname"), eosio::const_mem_fun<Edge, uint64_t, &Edge::by_from_node_edge_name_index>>,
                                   eosio::indexed_by<eosio::name("byfromto"), eosio::const_mem_fun<Edge, uint64_t, &Edge::by_from_node_to_node_index>>,
                                   eosio::indexed_by<eosio::name("bytoname"), eosio::const_mem_fun<Edge, uint64_t, &Edge::by_to_node_edge_name_index>>,
                                   eosio::indexed_by<eosio::name("bycreated"), eosio::const_mem_fun<Edge, uint64_t, &Edge::by_created>>,
                                   eosio::indexed_by<eosio::name("bycreator"), eosio::const_mem_fun<Edge, uint64_t, &Edge::by_creator>>>
            edge_table;
    };

} // namespace hypha