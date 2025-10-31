import requests
import threading
from datetime import datetime
import os

interval = 5 # in seconds
host = 'http://localhost'
port = 8000
path = 'PATH/TO/IMAGE'
url = f'{host}:{port}/{path}'
download_dir = '/PATH/TO/DOWNLOADS/DIRECTORY'

def downloadImg():
    # check if dir exists
    if not os.path.exists(f'{os.getcwd()}/{download_dir}'):
        os.makedirs(f'{os.getcwd()}/{download_dir}')
    data = requests.get(url).content
    img_name = datetime.now()
    with open(f'{download_dir}{img_name}.jpg', 'wb') as handler:
        handler.write(data)
    print(f'saved img to: {os.getcwd()}/{download_dir}{img_name}.jpg')
    threading.Timer(interval, downloadImg).start()

downloadImg()
