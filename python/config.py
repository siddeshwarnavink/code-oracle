import os

dataset_dir =  os.path.join(os.getcwd(), ".dataset")
output_dir =  os.path.join(os.getcwd(), ".output")

links_url = "https://www.geeksforgeeks.org/reactjs-projects/"
links_file_path = os.path.join(dataset_dir, "links.txt")

bpe_path = os.path.join(output_dir, "bpe.bin")
