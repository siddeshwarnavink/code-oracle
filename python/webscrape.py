import os
import time
import pathlib
import shutil
import uuid

from config import dataset_dir, links_url, links_file_path
from copypasta import gfg_table_links, gfg_scrape

# Initialization
start = time.time()
print("[INFO] Dataset path is", dataset_dir)

if os.path.isdir(dataset_dir):
    shutil.rmtree(pathlib.Path(dataset_dir))
    print("[INFO] Cleanup old dataset directory")

print("[INFO] Creating dataset directory")
os.mkdir(dataset_dir)

# Web scrape URL list
result = gfg_table_links(links_url, links_file_path)
if result == 1:
    quit()

# Web scrape code from each page
count = 0
with open(links_file_path, "r") as file:
    for link in file:
        clean_link = link.strip()
        out_file = os.path.join(dataset_dir, str(uuid.uuid1()) + ".js")
        result = gfg_scrape(clean_link, out_file)
        if result == 1:
            print("[INFO] Webpage skipped")
        else:
            count += 1

end = time.time()
elapsed_time =  end - start
print(f"Web scraped {count} pages in {elapsed_time:.2f} seconds")
