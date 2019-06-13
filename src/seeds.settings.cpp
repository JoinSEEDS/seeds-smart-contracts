#include <seeds.settings.hpp>

void settings::reset() {
  require_auth(_self);

  configure(name("tokenaccnt"), "placeholder"_n.value);
  configure(name("bankaccnt"), "placeholder"_n.value);
  configure(name("hrvstreward"), 0);
  configure(name("hrvstperiod"), 0);
  configure(name("subsperiod"), 0);
  configure(name("propsquota"), 0);
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
