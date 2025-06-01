import os
from config import bpe_path, model_path
from trashman import bpe_load, bpe_free, bpe_token_string
from jiraiya import load_model, rnn_predict

bpe_load(bpe_path)
if load_model(model_path) != 0:
    print(f"[ERROR] Failed to load model from {model_path}")
    exit(1)

print("Type 'exit' to quit")
while True:
    try:
        inp = input("~")
        if inp == "":
            continue
        elif inp == "exit":
            break
        pred = rnn_predict(inp)
        try:
            pred_id = int(pred)
            pred_str = bpe_token_string(pred_id)
        except Exception:
            pred_str = '<UNK>'
        print(f"Predicted next token: {pred_str} (id: {pred})")
    except KeyboardInterrupt:
        break

bpe_free()
