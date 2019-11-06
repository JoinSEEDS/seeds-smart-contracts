#include <seeds.settings.hpp>

void settings::reset() {
  require_auth(_self);

  configure(name("propminstake"), 25 * 10000);
  configure(name("refsnewprice"), 25 * 10000);
  configure(name("refsmajority"), 80);
  configure(name("refsquorum"), 80);
  configure(name("hrvstreward"), 100000);
}

void settings::configure(name param, uint64_t value) {
  require_auth(_self);

  auto citr = config.find(param.value);

  if (citr == config.end()) {
    config.emplace(_self, [&](auto& item) {
      item.param = param;
      item.value = value;
    });
  } else {
    config.modify(citr, _self, [&](auto& item) {
      item.param = param;
      item.value = value;
    });
  }
}
