import ctypes
from shared import cstr, cuint

lib = ctypes.CDLL("./.build/libtrashman.so")

lib.bpe_parse.argtypes = [ctypes.c_char_p]
lib.bpe_parse.restype = ctypes.c_int

lib.bpe_save.argtypes = [ctypes.c_char_p]

lib.bpe_load.argtypes = [ctypes.c_char_p]
lib.bpe_load.restype = ctypes.c_size_t

lib.bpe_test.argtypes = [ctypes.c_char_p]
lib.bpe_test.restype = ctypes.c_int

lib.bpe_free.argtypes = []
lib.bpe_token_string.argtypes = [ctypes.c_size_t]
lib.bpe_token_string.restype = ctypes.c_char_p

# --------------------------
# Python wrapper functions
# --------------------------

def bpe_parse(path: str) -> int:
    return lib.bpe_parse(cstr(path))

def bpe_save(path: str):
    lib.bpe_save(cstr(path))

def bpe_load(path: str) -> int:
    return lib.bpe_load(cstr(path))

def bpe_test(input: str) -> int:
    return lib.bpe_test(cstr(input))

def bpe_free():
    lib.bpe_free()

def bpe_token_string(token_id: int) -> str:
    s = lib.bpe_token_string(cuint(token_id))
    return s.decode('utf-8') if s else '<UNK>'
