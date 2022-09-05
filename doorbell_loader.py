import socket
from scipy.io import wavfile
import time


PORT = 80                   # The same port as used by the server
IP = "192.168.4.69"


def send_command(packet):
    assert len(packet) == (packet[0] << 8) + packet[1]

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((IP, PORT))

    s.sendall(bytes(packet))
    recv = s.recv(1024)
    print(f'Sent packet {packet[:4]}; recv: {recv}')
    assert recv == b'ack'

    s.close()

def cmd_play_audio(id):
    size = 4
    packet = bytes([(size >> 8) & 0xFF, size & 0xFF, 2, id])
    send_command(packet)

def cmd_delete_file(id):
    size = 4
    packet = bytes([(size >> 8) & 0xFF, size & 0xFF, 5, id])
    send_command(packet)

def cmd_print_str(s):
    s = bytes(s, 'utf-8')
    size = 3 + len(s)
    packet = bytes([(size >> 8) & 0xFF, size & 0xFF, 1]) + s
    send_command(packet)

def send_audio_file(file_id, filename):

    samplerate, data = wavfile.read(filename)  # sample_rate should be 16000 samp/s
    print(f'Sending {filename}, SR: {samplerate}, len: {len(data)}')
    data = [max(x, 1) for x in data]  # Convert to list and remove zeros
    # data = [x for x in data]  # Convert to list and remove zeros

    loop_counter = 0
    while len(data) > 0:
        chunk = data[:4000]
        data = data[len(chunk):]

        command = 4 if loop_counter else 3

        size = 4 + len(chunk)
        packet = [(size >> 8) & 0xFF, size & 0xFF, command, file_id] + chunk

        print(f'Packet {loop_counter}; Sending {len(chunk)} bytes...')
        send_command(packet)
        loop_counter += 1
        time.sleep(1.2)

    print('Done!')

if __name__ == '__main__':

    cmd_play_audio(3)
    # cmd_delete_ile(0)
    # cmd_delete_file(1)
    # cmd_delete_file(2)
    # cmd_delete_file(3)
    # cmd_delete_file(4)

    audio_to_send = [
        'ringtones/dingdong1.wav',
        'ringtones/ringaling4.wav',
        'ringtones/beepboop1.wav',
        'ringtones/blalala1.wav',
    ]
    #
    # for i, filename in enumerate(audio_to_send):
    #     send_audio_file(i, filename)

    # send_audio_file(4,  'ringtones/scuse_me.wav')
    # send_audio_file(0,  'ringtones/dingdong1.wav')
    # send_audio_file(1,   'ringtones/ringaling4.wav')
    # send_audio_file(2,   'ringtones/beepboop1.wav')
