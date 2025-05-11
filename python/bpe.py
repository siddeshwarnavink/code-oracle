import os
import ctypes

from shared import cstr, dataset_dir
from trashman import trashman

for filename in os.listdir(dataset_dir):
    file_path = os.path.join(dataset_dir, filename)
    trashman.bpe_parse(cstr(file_path))

trashman.bpe_print(0)
trashman.bpe_save(cstr(os.path.join(dataset_dir, "bpe.bin")))
trashman.bpe_free()
