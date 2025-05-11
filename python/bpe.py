import os
import ctypes

from shared import cstr, dataset_dir

trashman = ctypes.CDLL("./.build/libtrashman.so")

trashman.parse_code.argtypes = [ctypes.c_char_p]
trashman.parse_code.restype = ctypes.c_int
trashman.parse_code_print.argtypes = [ctypes.c_size_t]
trashman.parse_code_save.argtypes = [ctypes.c_char_p]
trashman.parse_code_free.argtypes = []

for filename in os.listdir(dataset_dir):
    file_path = os.path.join(dataset_dir, filename)
    trashman.parse_code(cstr(file_path))

trashman.parse_code_print(0)
trashman.parse_code_save(cstr(os.path.join(dataset_dir, "bpe.bin")))
trashman.parse_code_free()
