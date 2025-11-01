import requests
import threading
from datetime import datetime
import os

interval = 5 # in seconds
host = 'http://localhost'
port = 8000
path = 'PATH/TO/IMAGE'
url = f'{host}:{port}/{path}'
download_dir = 'RELATIVE/PATH/TO/DOWNLOADS/DIRECTORY'

def downloadImg():
    # check if dir exists
    save_dir = os.path.join(os.getcwd(), download_dir)
    os.makedirs(save_dir, exist_ok=True)
    data = requests.get(url).content
    img_name = datetime.now().strftime("%Y%m%d_%H%M%S")
    file_path = os.path.join(save_dir, f"{img_name}.jpg")

    with open(file_path, 'wb') as handler:
        handler.write(data)

    print(f"Saved image to: {file_path}")
    threading.Timer(interval, downloadImg).start()
