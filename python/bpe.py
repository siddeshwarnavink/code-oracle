import ctypes

from shared import cstr

trashman = ctypes.CDLL("./.build/libtrashman.so")

trashman.parse_code.argtypes = [ctypes.c_char_p]
trashman.parse_code.restype = ctypes.c_int

trashman.parse_code(cstr("./.dataset/748faecd-2a29-11f0-8144-9c6b00022bd0.js"))
