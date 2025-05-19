import time
import shutil
import os
import pathlib

from config import dataset_dir, bpe_path, output_dir
from trashman import bpe_parse, bpe_save, bpe_free

start = time.time()
print("[INFO] Output path is", bpe_path)

if not os.path.isdir(output_dir):
    print("[INFO] Creating output directory")
    os.mkdir(output_dir)

for filename in os.listdir(dataset_dir):
    file_path = os.path.join(dataset_dir, filename)
    bpe_parse(file_path)

bpe_save(bpe_path)
bpe_free()

end = time.time()
elapsed_time =  end - start
print(f"BPE done in {elapsed_time:.2f} seconds.")
