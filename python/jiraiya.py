import ctypes
from shared import cuint, cstr

lib = ctypes.CDLL("./.build/libjiraiya.so")

lib.rnn.argtypes = [ctypes.c_size_t, ctypes.c_size_t, ctypes.c_size_t, ctypes.c_size_t, ctypes.c_char_p]
lib.rnn.restype = ctypes.c_int

lib.load_model.argtypes = [ctypes.c_char_p]
lib.load_model.restype = ctypes.c_int

lib.rnn_predict.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_size_t]
lib.rnn_predict.restype = ctypes.c_int

def rnn(vocab_size, embedding_dim, hidden_layers, epochs, model_path):
    return lib.rnn(cuint(vocab_size), cuint(embedding_dim), cuint(hidden_layers), cuint(epochs), cstr(model_path))

def load_model(model_path: str) -> int:
    return lib.load_model(cstr(model_path))

def rnn_predict(input_str: str, output_len: int = 128) -> str:
    buf = ctypes.create_string_buffer(output_len)
    lib.rnn_predict(cstr(input_str), buf, cuint(output_len))
    return buf.value.decode('utf-8')
