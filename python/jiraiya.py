import ctypes
from shared import cuint

lib = ctypes.CDLL("./.build/libjiraiya.so")

lib.rnn.argtypes = [ctypes.c_size_t, ctypes.c_size_t, ctypes.c_size_t, ctypes.c_size_t]
lib.rnn.restype = ctypes.c_int

def rnn(vocab_size, embedding_dim, hidden_layers, epochs):
    return lib.rnn(cuint(vocab_size), cuint(embedding_dim), cuint(hidden_layers), cuint(epochs))

