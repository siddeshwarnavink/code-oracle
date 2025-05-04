import os
import pathlib
import shutil
import ctypes
import uuid

# Configuration
dataset_dir =  os.path.join(os.getcwd(), ".dataset")

links_url = "https://www.geeksforgeeks.org/reactjs-projects/"
links_file_path = os.path.join(dataset_dir, "links.txt")

# Load library
copypasta = ctypes.CDLL("./.build/libcopypasta.so")

copypasta.gfg_table_links.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
copypasta.gfg_table_links.restype = ctypes.c_int

copypasta.gfg_scrape.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
copypasta.gfg_scrape.restype = ctypes.c_int

def cstr(s):
    return ctypes.c_char_p(s.encode("utf-8"))

# Initialization
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
with open(links_file_path, "r") as file:
    for link in file:
        clean_link = link.strip()
        out_file = os.path.join(dataset_dir, str(uuid.uuid1()) + ".js")
        result = copypasta.gfg_scrape(cstr(clean_link), cstr(out_file))
        if result == 1:
            print("[INFO] Webpage skipped")
