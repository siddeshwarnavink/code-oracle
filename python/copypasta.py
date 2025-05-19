import ctypes

from shared import cstr

lib = ctypes.CDLL("./.build/libcopypasta.so")

lib.gfg_table_links.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
lib.gfg_table_links.restype = ctypes.c_int

lib.gfg_scrape.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
lib.gfg_scrape.restype = ctypes.c_int

# --------------------------
# Python wrapper functions
# --------------------------

def gfg_table_links(url: str, path: str) -> int:
    return lib.gfg_table_links(cstr(url), cstr(path))

def gfg_scrape(url: str, path: str) -> int:
    return lib.gfg_scrape(cstr(url), cstr(path))
