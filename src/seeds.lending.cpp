#include <seeds.lending.hpp>

void lending::reset() {
    require_auth(get_self());

    config.remove();

    config_table c = config.get_or_create(get_self(), config_table());

    c.rate = asset(100000, seeds_symbol);
    c.fee = 10;

    config.set(c, get_self());
}

void lending::updaterate(asset rate) {
    require_auth(get_self());

    config_table c = config.get_or_create(get_self(), config_table());

    c.rate = rate;
}

void lending::updatefee(uint64_t fee) {
    require_auth(get_self());

    config_table c = config.get_or_create(get_self(), config_table());

    c.fee = fee;
}

void lending::borrow(name account, name contract, asset tlos_quantity, string memo) {
    if (contract != get_self()) return;

    config_table c = config.get();
    asset rate = c.rate;

    uint64_t seeds_amount = (tlos_quantity.amount * rate.amount) / 10000;
    asset seeds_quantity = asset(seeds_amount, seeds_symbol);

    action(
        permission_level{get_self(), "active"_n},
        contracts::token, "transfer"_n,
        make_tuple(get_self(), account, seeds_quantity, string(""))
    ).send();
}

void lending::refund(name account, name contract, asset seeds_quantity, string memo) {
    if (contract != get_self()) return;

    config_table c = config.get();
    asset rate = c.rate;
    uint64_t fee = c.fee;

    uint64_t tlos_amount = (seeds_quantity.amount / rate.amount);
    asset tlos_quantity = asset(tlos_amount, seeds_symbol);

    auto bitr = balances.find(account.value);
    check(bitr != balances.end(), "user has no deposit");
    asset tlos_deposit = bitr->tlos_deposit;
    check(tlos_deposit >= tlos_quantity, "refund limit overdrawn (max: " + tlos_deposit.to_string() + " )");

    balances.modify(bitr, get_self(), [&](auto& balance) {
        balance.tlos_deposit -= tlos_quantity;
    });

    uint64_t tlos_refund_amount = tlos_amount - (tlos_amount * fee / 100);
    asset tlos_refund_quantity = asset(tlos_refund_amount, tlos_symbol);

    action(
        permission_level{get_self(), "active"_n},
        contracts::tlostoken, "transfer"_n,
        make_tuple(get_self(), account, tlos_quantity, string(""))
    ).send();
}