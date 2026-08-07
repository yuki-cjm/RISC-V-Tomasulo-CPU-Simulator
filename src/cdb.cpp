#include "cdb.hpp"

CDB_Entry make_empty_cdb() {
    return CDB_Entry{false, 0, -1, false, 0, -1};
}