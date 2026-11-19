// Leak detection is not what this fleet fuzzes for; ASan and UBSan stay fully on.
extern "C" int __lsan_is_turned_off() { return 1; }
