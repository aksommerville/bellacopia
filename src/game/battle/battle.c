#include "game/bellacopia.h"

#define _(tag) extern const struct battle_type battle_type_##tag;
FOR_EACH_battle
#undef _

const struct battle_type *battle_type_by_id(int battle) {
  switch (battle) {
    #define _(tag) case NS_battle_##tag: return &battle_type_##tag;
    FOR_EACH_battle
    #undef _
  }
  return 0;
}
