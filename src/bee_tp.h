#include "src/third_party/tp.h"

struct tp *bee_tp_new(smart_string *s, int is_persistent);
void bee_tp_free(struct tp* tps, int is_persistent);
void bee_tp_flush(struct tp* tps);
void bee_tp_update(struct tp* tps);
