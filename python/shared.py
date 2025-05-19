import ctypes

def cuint(i):
    return ctypes.c_size_t(i)

def cstr(s):
    return ctypes.c_char_p(s.encode("utf-8"))
