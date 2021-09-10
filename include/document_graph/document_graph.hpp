#pragma once

#include <cstring>

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/crypto.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/name.hpp>

#include <document_graph/content.hpp>
#include <document_graph/document.hpp>
#include <document_graph/edge.hpp>

namespace hypha
{
    class DocumentGraph
    {
    public:
        DocumentGraph(const eosio::name &contract) : m_contract(contract) {}
        ~DocumentGraph() {}

        void removeEdges(const eosio::checksum256 &node);

        std::vector<Edge> getEdges(const eosio::checksum256 &fromNode, const eosio::checksum256 &toNode);
        std::vector<Edge> getEdgesOrFail(const eosio::checksum256 &fromNode, const eosio::checksum256 &toNode);

        std::vector<Edge> getEdgesFrom(const eosio::checksum256 &fromNode, const eosio::name &edgeName);
        std::vector<Edge> getEdgesFromOrFail(const eosio::checksum256 &fromNode, const eosio::name &edgeName);

        std::vector<Edge> getEdgesTo(const eosio::checksum256 &toNode, const eosio::name &edgeName);
        std::vector<Edge> getEdgesToOrFail(const eosio::checksum256 &toNode, const eosio::name &edgeName);

        Edge createEdge(eosio::name &creator, const eosio::checksum256 &fromNode, const eosio::checksum256 &toNode, const eosio::name &edgeName);

        Document updateDocument(const eosio::name &updater,
                                const eosio::checksum256 &doc_hash,
                                ContentGroups content_groups);

        void replaceNode(const eosio::checksum256 &oldNode, const eosio::checksum256 &newNode);
        void eraseDocument(const eosio::checksum256 &document_hash);
        void eraseDocument(const eosio::checksum256 &document_hash, const bool includeEdges);

    private:
        eosio::name m_contract;
    };
}; // namespace hypha

#define DECLARE_DOCUMENT_GRAPH(contract)\
using FlexValue = hypha::Content::FlexValue;\
using root_doc = hypha::Document;\
TABLE contract##_document : public root_doc {};\
using contract_document = contract##_document;\
using document_table =  eosio::multi_index<eosio::name("documents"), contract_document,\
                            eosio::indexed_by<name("idhash"), eosio::const_mem_fun<root_doc, eosio::checksum256, &root_doc::by_hash>>,\
                            eosio::indexed_by<name("bycreator"), eosio::const_mem_fun<root_doc, uint64_t, &root_doc::by_creator>>,\
                            eosio::indexed_by<name("bycreated"), eosio::const_mem_fun<root_doc, uint64_t, &root_doc::by_created>>>;\
using root_edge = hypha::Edge;\
TABLE contract##_edge : public root_edge {};\
using contract_edge = contract##_edge;\
using edge_table = eosio::multi_index<eosio::name("edges"), contract_edge,\
            eosio::indexed_by<eosio::name("fromnode"), eosio::const_mem_fun<root_edge, eosio::checksum256, &root_edge::by_from>>,\
            eosio::indexed_by<eosio::name("tonode"), eosio::const_mem_fun<root_edge, eosio::checksum256, &root_edge::by_to>>,\
            eosio::indexed_by<eosio::name("edgename"), eosio::const_mem_fun<root_edge, uint64_t, &root_edge::by_edge_name>>,\
            eosio::indexed_by<eosio::name("byfromname"), eosio::const_mem_fun<root_edge, uint64_t, &root_edge::by_from_node_edge_name_index>>,\
            eosio::indexed_by<eosio::name("byfromto"), eosio::const_mem_fun<root_edge, uint64_t, &root_edge::by_from_node_to_node_index>>,\
            eosio::indexed_by<eosio::name("bytoname"), eosio::const_mem_fun<root_edge, uint64_t, &root_edge::by_to_node_edge_name_index>>,\
            eosio::indexed_by<eosio::name("bycreated"), eosio::const_mem_fun<root_edge, uint64_t, &root_edge::by_created>>,\
            eosio::indexed_by<eosio::name("bycreator"), eosio::const_mem_fun<root_edge, uint64_t, &root_edge::by_creator>>>;