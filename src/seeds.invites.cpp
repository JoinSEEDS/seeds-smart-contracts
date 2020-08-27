#include <seeds.invites.hpp>

void invites::send(name from, name to, asset quantity, string memo) {
  if (to == get_self()) {
    check(false, "this contract has been disabled. Please use join.seeds for invitations to SEEDS!");
  }
}
