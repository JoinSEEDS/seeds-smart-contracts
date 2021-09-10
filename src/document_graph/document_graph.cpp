#include <document_graph/document_graph.hpp>
#include <document_graph/document.hpp>
#include <document_graph/util.hpp>

namespace hypha
{
    std::vector<Edge> DocumentGraph::getEdges(const eosio::checksum256 &fromNode, const eosio::checksum256 &toNode)
    {
        std::vector<Edge> edges;

        // this index uniquely identifies all edges that share this fromNode and toNode
        uint64_t index = concatHash(fromNode, toNode);
        Edge::edge_table e_t(m_contract, m_contract.value);
        auto from_name_index = e_t.get_index<eosio::name("byfromto")>();
        auto itr = from_name_index.find(index);

        while (itr != from_name_index.end() && itr->by_from_node_to_node_index() == index)
        {
            edges.push_back(*itr);
            itr++;
        }

        return edges;
    }

    std::vector<Edge> DocumentGraph::getEdgesOrFail(const eosio::checksum256 &fromNode, const eosio::checksum256 &toNode)
    {
        std::vector<Edge> edges = getEdges(fromNode, toNode);
        eosio::check(edges.size() > 0, "no edges exist: from " + readableHash(fromNode) + " to " + readableHash(toNode));
        return edges;
    }

    std::vector<Edge> DocumentGraph::getEdgesFrom(const eosio::checksum256 &fromNode, const eosio::name &edgeName)
    {
        std::vector<Edge> edges;

        // this index uniquely identifies all edges that share this fromNode and edgeName
        uint64_t index = concatHash(fromNode, edgeName);
        Edge::edge_table e_t(m_contract, m_contract.value);
        auto from_name_index = e_t.get_index<eosio::name("byfromname")>();
        auto itr = from_name_index.find(index);

        while (itr != from_name_index.end() && itr->by_from_node_edge_name_index() == index)
        {
            edges.push_back(*itr);
            itr++;
        }

        return edges;
    }

    std::vector<Edge> DocumentGraph::getEdgesFromOrFail(const eosio::checksum256 &fromNode, const eosio::name &edgeName)
    {
        std::vector<Edge> edges = getEdgesFrom(fromNode, edgeName);
        eosio::check(edges.size() > 0, "no edges exist: from " + readableHash(fromNode) + " with name " + edgeName.to_string());
        return edges;
    }

    std::vector<Edge> DocumentGraph::getEdgesTo(const eosio::checksum256 &toNode, const eosio::name &edgeName)
    {
        std::vector<Edge> edges;

        // this index uniquely identifies all edges that share this toNode and edgeName
        uint64_t index = concatHash(toNode, edgeName);
        Edge::edge_table e_t(m_contract, m_contract.value);
        auto from_name_index = e_t.get_index<eosio::name("bytoname")>();
        auto itr = from_name_index.find(index);

        while (itr != from_name_index.end() && itr->by_to_node_edge_name_index() == index)
        {
            edges.push_back(*itr);
            itr++;
        }

        return edges;
    }

    std::vector<Edge> DocumentGraph::getEdgesToOrFail(const eosio::checksum256 &toNode, const eosio::name &edgeName)
    {
        std::vector<Edge> edges = getEdgesTo(toNode, edgeName);
        eosio::check(edges.size() > 0, "no edges exist: to " + readableHash(toNode) + " with name " + edgeName.to_string());
        return edges;
    }

    // since we are removing multiple edges here, we do not call erase on each edge, which
    // would instantiate the table on each call.  This is faster execution.
    void DocumentGraph::removeEdges(const eosio::checksum256 &node)
    {
        Edge::edge_table e_t(m_contract, m_contract.value);

        auto from_node_index = e_t.get_index<eosio::name("fromnode")>();
        auto from_itr = from_node_index.find(node);

        while (from_itr != from_node_index.end() && from_itr->to_node == node)
        {
            from_itr = from_node_index.erase(from_itr);
        }

        auto to_node_index = e_t.get_index<eosio::name("tonode")>();
        auto to_itr = to_node_index.find(node);

        while (to_itr != to_node_index.end() && to_itr->to_node == node)
        {
            to_itr = to_node_index.erase(to_itr);
        }
    }

    void DocumentGraph::replaceNode(const eosio::checksum256 &oldNode, const eosio::checksum256 &newNode)
    {
        Edge::edge_table e_t(m_contract, m_contract.value);

        auto from_node_index = e_t.get_index<eosio::name("fromnode")>();
        auto from_itr = from_node_index.find(oldNode);

        while (from_itr != from_node_index.end() && from_itr->from_node == oldNode)
        {
            // create the new edge record
            Edge newEdge(m_contract, m_contract, newNode, from_itr->to_node, from_itr->edge_name);

            // erase the old edge record
            from_itr = from_node_index.erase(from_itr);
        }

        auto to_node_index = e_t.get_index<eosio::name("tonode")>();
        auto to_itr = to_node_index.find(oldNode);

        while (to_itr != to_node_index.end() && to_itr->to_node == oldNode)
        {
            // create the new edge record
            Edge newEdge(m_contract, m_contract, to_itr->from_node, newNode, to_itr->edge_name);

            // erase the old edge record
            to_itr = to_node_index.erase(to_itr);
        }
    }

    Document DocumentGraph::updateDocument(const eosio::name &updater,
                                           const eosio::checksum256 &documentHash,
                                           ContentGroups contentGroups)
    {
        Document currentDocument(m_contract, documentHash);
        Document newDocument(m_contract, updater, contentGroups);

        replaceNode(documentHash, newDocument.getHash());
        eraseDocument(documentHash, false);
        return newDocument;
    }

    // for now, permissions should be handled in the contract action rather than this class
    void DocumentGraph::eraseDocument(const eosio::checksum256 &documentHash, const bool includeEdges)
    {
        Document::document_table d_t(m_contract, m_contract.value);
        auto hash_index = d_t.get_index<eosio::name("idhash")>();
        auto h_itr = hash_index.find(documentHash);

        eosio::check(h_itr != hash_index.end(), "Cannot erase document; does not exist: " + readableHash(documentHash));

        if (includeEdges)
        {
            removeEdges(documentHash);
        }

        hash_index.erase(h_itr);
    }

    void DocumentGraph::eraseDocument(const eosio::checksum256 &documentHash)
    {
        return eraseDocument(documentHash, true);
    }
} // namespace hypha