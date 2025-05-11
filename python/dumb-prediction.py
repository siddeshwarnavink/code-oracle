import os
import ctypes

from shared import cstr, dataset_dir
from trashman import trashman

trashman.bpe_load(cstr(os.path.join(dataset_dir, "bpe.bin")))

print("Type 'exit' to quit")
while True:
    try:
        inp = input("~")
        if inp == "":
            continue
        elif inp == "exit":
            break
        result = trashman.bpe_test(cstr(inp))
        if result == 1:
            print("An Error occured!")
    except KeyboardInterrupt:
        break

trashman.bpe_free()
