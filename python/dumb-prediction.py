import os
import ctypes

from config import dataset_dir
from trashman import bpe_load, bpe_test, bpe_free 

bpe_load(os.path.join(dataset_dir, "bpe.bin"))

print("Type 'exit' to quit")
while True:
    try:
        inp = input("~")
        if inp == "":
            continue
        elif inp == "exit":
            break
        result = bpe_test(inp)
        if result == 1:
            print("An Error occured!")
    except KeyboardInterrupt:
        break

bpe_free()
