from websocket import create_connection
from pynput import keyboard

esp_ip = '192.168.4.1'
url = f'ws://{esp_ip}:8000/ws'

ws = create_connection(url)
print(f'connected to {url}')

pressed = set()
last_sent = None

def make_cmd():
    fwd = 'w' in pressed or 'W' in pressed or 'ArrowUp' in pressed
    back = 's' in pressed or 'S' in pressed or 'ArrowDown' in pressed
    left = 'a' in pressed or 'A' in pressed or 'ArrowLeft' in pressed
    right = 'd' in pressed or 'D' in pressed or 'ArrowRight' in pressed

    if fwd and back:
        return "S"

    if fwd and left:
        return "FL"
    if fwd and right:
        return "FR"
    if fwd:
        return "F"

    if back and left:
        return "BL"
    if back and right:
        return "BR"
    if back:
        return "B"

    if left:
        return "L"
    if right:
        return "R"

    return "S"

def send_cmd(cmd):
    global last_sent
    if cmd == last_sent:
        return
    ws.send(cmd)
    last_sent = cmd
    print("sent:", cmd)

def on_press(key):
    try:
        k = key.char.lower()
    except:
        return
    if k in ('w', 'a', 's', 'd'):
      pressed.add(k)
      cmd = make_cmd()
      send_cmd(cmd)

def on_release(key):
    try:
        k = key.char.lower()
    except:
        return
    if k in pressed:
        pressed.remove(k)
        cmd = make_cmd()
        send_cmd(cmd)

with keyboard.Listener(on_press=on_press, on_release=on_release) as listener:
    listener.join()