#include "wal.hpp"
#include <cassert>
#include <cstdio>
int main() { const std::string path = "test_adaptivex.wal"; WALConfig cfg; cfg.path = path; cfg.batch_ms = 1; { WAL wal(cfg); wal.write({LogPriority::CRITICAL, "{\"fill\":1}", std::chrono::steady_clock::now()}); wal.stop(); } auto records = WAL::recover(path); assert(records.size() == 1); std::remove(path.c_str()); return 0; }
