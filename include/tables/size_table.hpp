#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_SIZE_TABLE TABLE size_table { \
        name id; \
        uint64_t size; \
        uint64_t primary_key()const { return id.value; } \
      };

#define DEFINE_SIZE_TABLE_MULTI_INDEX typedef eosio::multi_index<"sizes"_n, size_table> size_tables;

#define DEFINE_SIZE_CHANGE \
      void size_change(name id, int delta) { \
        auto sitr = sizes.find(id.value); \
        if (sitr == sizes.end()) { \
          sizes.emplace(_self, [&](auto& item) { \
            item.id = id; \
            item.size = delta; \
          }); \
        } else { \
          uint64_t newsize = sitr->size + delta; \
          if (delta < 0) { \
            if (sitr->size < -delta) { \
              newsize = 0; \
            } \
          } \
          sizes.modify(sitr, _self, [&](auto& item) { \
            item.size = newsize; \
          }); \
        } \
      }

#define DEFINE_SIZE_SET \
      void size_set(name id, uint64_t newsize) { \
        auto sitr = sizes.find(id.value); \
        if (sitr == sizes.end()) { \
          sizes.emplace(_self, [&](auto& item) { \
            item.id = id; \
            item.size = newsize; \
          }); \
        } else { \
          sizes.modify(sitr, _self, [&](auto& item) { \
            item.size = newsize; \
          }); \
        } \
      } 

#define DEFINE_SIZE_GET \
      uint64_t get_size(name id) { \
        auto sitr = sizes.find(id.value); \
        if (sitr == sizes.end()) { \
          return 0; \
        } else { \
          return sitr->size; \
        } \
      } 
