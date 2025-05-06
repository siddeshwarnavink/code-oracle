import os
import time
import pathlib
import shutil
import ctypes
import uuid

from shared import cstr, dataset_dir, links_url, links_file_path

# Load library
copypasta = ctypes.CDLL("./.build/libcopypasta.so")

copypasta.gfg_table_links.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
copypasta.gfg_table_links.restype = ctypes.c_int

copypasta.gfg_scrape.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
copypasta.gfg_scrape.restype = ctypes.c_int

# Initialization
start = time.time()
print("[INFO] Dataset path is", dataset_dir)

if os.path.isdir(dataset_dir):
    shutil.rmtree(pathlib.Path(dataset_dir))
    print("[INFO] Cleanup old dataset directory")

print("[INFO] Creating dataset directory")
os.mkdir(dataset_dir)

# Web scrape URL list
result = copypasta.gfg_table_links(cstr(links_url), cstr(links_file_path))
if result == 1:
    quit()

# Web scrape code from each page
count = 0
with open(links_file_path, "r") as file:
    for link in file:
        clean_link = link.strip()
        out_file = os.path.join(dataset_dir, str(uuid.uuid1()) + ".js")
        result = copypasta.gfg_scrape(cstr(clean_link), cstr(out_file))
        if result == 1:
            print("[INFO] Webpage skipped")
        else:
            count += 1

end = time.time()
elapsed_time =  end - start
print(f"Web scraped {count} pages in {elapsed_time:.2f} seconds")
