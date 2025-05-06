import ctypes
import os

# Configuration
dataset_dir =  os.path.join(os.getcwd(), ".dataset")

links_url = "https://www.geeksforgeeks.org/reactjs-projects/"
links_file_path = os.path.join(dataset_dir, "links.txt")

def cstr(s):
    return ctypes.c_char_p(s.encode("utf-8"))
