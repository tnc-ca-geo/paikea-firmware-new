#include <scoutMessages.h>

void ScoutMessages::createPK001(SystemState &state, char* bfr) {
    memcpy(bfr, (char*) "PK001;lat:", 10);
}