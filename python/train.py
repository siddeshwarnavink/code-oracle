import os
import ctypes

from config import bpe_path
from trashman import bpe_load, bpe_free
from jiraiya import rnn

# ---------------------------
# Model Config
# ---------------------------

embedding_dim = 128
hidden_layers = 32
epochs = 10
sequence_length = 32

# ---------------------------
# Training Code
# ---------------------------

tokens_count = bpe_load(bpe_path)

if rnn(tokens_count, embedding_dim, hidden_layers, epochs) > 0:
    print("Training model failed!")

bpe_free()
