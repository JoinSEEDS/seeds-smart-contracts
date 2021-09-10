#include <eosio/crypto.hpp>

#include <map>

#include <document_graph/document.hpp>
#include <document_graph/util.hpp>

namespace hypha
{

    Document::~Document() {}
    Document::Document() {}

    Document::Document(eosio::name contract, eosio::name creator, ContentGroups contentGroups)
        : contract{contract}, creator{creator}, content_groups{std::move(contentGroups)}
    {
        emplace();
    }

    Document::Document(eosio::name contract, eosio::name creator, ContentGroup contentGroup)
        : Document(contract, creator, rollup(contentGroup))
    {
    }

    Document::Document(eosio::name contract, eosio::name creator, Content content)
        : Document(contract, creator, rollup(content))
    {
    }

    Document::Document(eosio::name contract, eosio::name creator, const std::string &label, const Content::FlexValue &value)
        : Document(contract, creator, rollup(Content(label, value)))
    {
    }

    Document::Document(eosio::name contract, const eosio::checksum256 &_hash) : contract{contract}
    {
        document_table d_t(contract, contract.value);
        auto hash_index = d_t.get_index<eosio::name("idhash")>();
        auto h_itr = hash_index.find(_hash);
        eosio::check(h_itr != hash_index.end(), "document not found: " + readableHash(_hash));

        id = h_itr->id;
        creator = h_itr->creator;
        created_date = h_itr->created_date;
        certificates = h_itr->certificates;
        content_groups = h_itr->content_groups;
        hashContents();

        // this should never happen, only if hash algorithm somehow changed
        eosio::check(hash == _hash, "fatal error: provided and indexed hash does not match newly generated hash");
    }

    bool Document::exists(eosio::name contract, const eosio::checksum256 &_hash)
    {
        document_table d_t(contract, contract.value);
        auto hash_index = d_t.get_index<eosio::name("idhash")>();
        auto h_itr = hash_index.find(_hash);

        if (h_itr != hash_index.end())
        {
            return true;
        }
        return false;
    }

    void Document::emplace()
    {
        hashContents();

        document_table d_t(getContract(), getContract().value);
        auto hash_index = d_t.get_index<eosio::name("idhash")>();
        auto h_itr = hash_index.find(hash);

        // if this content exists already, error out and send back the hash of the existing document
        eosio::check(h_itr == hash_index.end(), "document exists already: " + readableHash(hash));

        d_t.emplace(getContract(), [&](auto &d) {
            id = d_t.available_primary_key();
            created_date = eosio::current_time_point();
            d = *this;
        });
    }

    Document Document::getOrNew(eosio::name _contract, eosio::name _creator, ContentGroups contentGroups)
    {
        Document document{};
        document.content_groups = contentGroups;
        document.hashContents();

        Document::document_table d_t(_contract, _contract.value);
        auto hash_index = d_t.get_index<eosio::name("idhash")>();
        auto h_itr = hash_index.find(document.hash);

        // if this content exists already, return this one
        if (h_itr != hash_index.end())
        {
            document.contract = _contract;
            document.creator = h_itr->creator;
            document.created_date = h_itr->created_date;
            document.certificates = h_itr->certificates;
            document.id = h_itr->id;
            return document;
        }

        return Document(_contract, _creator, contentGroups);
    }

    Document Document::getOrNew(eosio::name contract, eosio::name creator, ContentGroup contentGroup)
    {
        return getOrNew(contract, creator, rollup(contentGroup));
    }

    Document Document::getOrNew(eosio::name contract, eosio::name creator, Content content)
    {
        return getOrNew(contract, creator, rollup(content));
    }

    Document Document::getOrNew(eosio::name contract, eosio::name creator, const std::string &label, const Content::FlexValue &value)
    {
        return getOrNew(contract, creator, rollup(Content(label, value)));
    }

    // void Document::certify(const eosio::name &certifier, const std::string &notes)
    // {
    //     // check if document is already saved??
    //     document_table d_t(m_contract, m_contract.value);
    //     auto h_itr = hash_index.find(id);
    //     eosio::check(h_itr != d_t.end(), "document not found when attemption to certify: " + readableHash(geash()));

    //     require_auth(certifier);

    //     // TODO: should a certifier be able to sign the same document fork multiple times?
    //     d_t.modify(h_itr, m_contract, [&](auto &d) {
    //         d = std::move(this);
    //         d.certificates.push_back(new_certificate(certifier, notes));
    //     });
    // }

    const void Document::hashContents()
    {
        // save/cache the hash in the member
        hash = hashContents(content_groups);
    }

    const std::string Document::toString()
    {
        return toString(content_groups);
    }

    // static version cannot cache the hash in a member
    const eosio::checksum256 Document::hashContents(const ContentGroups &contentGroups)
    {
        std::string string_data = toString(contentGroups);
        return eosio::sha256(const_cast<char *>(string_data.c_str()), string_data.length());
    }

    const std::string Document::toString(const ContentGroups &contentGroups)
    {
        std::string results = "[";
        bool is_first = true;

        for (const ContentGroup &contentGroup : contentGroups)
        {
            if (is_first)
            {
                is_first = false;
            }
            else
            {
                results = results + ",";
            }
            results = results + toString(contentGroup);
        }

        results = results + "]";
        return results;
    }

    const std::string Document::toString(const ContentGroup &contentGroup)
    {
        std::string results = "[";
        bool is_first = true;

        for (const Content &content : contentGroup)
        {
            if (is_first)
            {
                is_first = false;
            }
            else
            {
                results = results + ",";
            }
            results = results + content.toString();
        }

        results = results + "]";
        return results;
    }

    ContentGroups Document::rollup(ContentGroup contentGroup)
    {
        ContentGroups contentGroups;
        contentGroups.push_back(contentGroup);
        return contentGroups;
    }

    ContentGroups Document::rollup(Content content)
    {
        ContentGroup contentGroup;
        contentGroup.push_back(content);
        return rollup(contentGroup);
    }

    Document Document::merge(Document original, Document &deltas)
    {
      const auto& deltasGroups = deltas.getContentGroups();
      auto& originalGroups = original.getContentGroups();
      auto deltasWrapper = deltas.getContentWrapper();
      auto originalWrapper = original.getContentWrapper();

      //unordered_map not available with eosio atm
      std::map<string, std::pair<size_t, ContentGroup*>> groupsByLabel;

      for (size_t i = 0; i < originalGroups.size(); ++i) {
        auto label = ContentWrapper::getGroupLabel(originalGroups[i]);
        if (!label.empty()) {
          groupsByLabel[string(label)] = std::pair{i, &originalGroups[i]};
        }
      }  

      for (size_t i = 0; i < deltasGroups.size(); ++i) {
        
        auto label = ContentWrapper::getGroupLabel(deltasGroups[i]);
        
        //If there is no group label just append it to the original doc
        if (label.empty()) {
          originalGroups.push_back(deltasGroups[i]);
          continue;
        }
        
        //Check if we need to delete the group
        if (auto [idx, c] = deltasWrapper.get(i, "delete_group"); 
            c) {
          originalWrapper.removeGroup(string(label));
          continue;
        }
        
        //If group is not present on original document we should append it
        if (auto groupIt = groupsByLabel.find(string(label)); 
            groupIt == groupsByLabel.end()) {
          originalGroups.push_back(deltasGroups[i]);
        }
        else {
          auto [oriGroupIdx, oriGroup] = groupIt->second;

          //It doesn't matter if it replaces content_group_label as they should be equal
          for (auto& deltaContent : deltasGroups[i]) {
            if (std::holds_alternative<std::monostate>(deltaContent.value)) {
              originalWrapper.removeContent(oriGroupIdx, deltaContent.label);
            }
            else {
              originalWrapper.insertOrReplace(oriGroupIdx, deltaContent);
            }
          }
        }
      }

      return original;
    }
} // namespace hypha
