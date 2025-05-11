import ctypes

trashman = ctypes.CDLL("./.build/libtrashman.so")

trashman.bpe_parse.argtypes = [ctypes.c_char_p]
trashman.bpe_parse.restype = ctypes.c_int

trashman.bpe_print.argtypes = [ctypes.c_size_t]

trashman.bpe_save.argtypes = [ctypes.c_char_p]

trashman.bpe_load.argtypes = [ctypes.c_char_p]

trashman.bpe_test.argtypes = [ctypes.c_char_p]
trashman.bpe_test.restype = ctypes.c_int

trashman.bpe_free.argtypes = []
