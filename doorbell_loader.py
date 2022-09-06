import socket
from scipy.io import wavfile
import time


PORT = 80                   # The same port as used by the server
IP = "192.168.4.69"

AUTH_FILE = "doorbell/auth_tokens.h"
HEADER_SIZE = 1 + 2 + 8 + 1  # checksum + size + authentication token + command


def calc_checksum(packet):
    return  bytes([(~sum(packet) + 1) & 0xFF])

def load_auth_token(filename):

    with open(filename) as auth_file:
        for line in auth_file:
            if '###TOKEN###' in line and 'token_admin' in line:
                token_str = line.strip().split('}')[0].replace('{', '')
                token_bytes = [int(b, 16) for b in token_str.split(',') ]
                return bytes(token_bytes)


def send_command(packet):
    assert len(packet) == (packet[1] << 8) + packet[2]

    while True:  # Retry sending packet until it succeeds

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((IP, PORT))

        s.sendall(bytes(packet))
        recv = s.recv(1024)
        print(f'Sent packet {packet[:4]}; recv: {recv}')

        if recv == b'ACK\r\n':
            s.close()
            break;

        print(f'Error sending packet: {recv}')

        s.close()
        time.sleep(0.5)

def build_packet(cmd, payload):
    payload = bytes(payload)
    size = HEADER_SIZE + len(payload)
    assert size < 4096, "Packet too large"

    packet = bytes([(size >> 8) & 0xFF, size & 0xFF]) + load_auth_token(AUTH_FILE) + bytes([cmd]) + payload
    return calc_checksum(packet) + packet

def cmd_play_audio(id):
    send_command(build_packet(2, [id]))

def cmd_delete_file(id):
    send_command(build_packet(5, [id]))

def cmd_print_str(s):
    s = bytes(s, 'utf-8')
    send_command(build_packet(1, s))

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

        print(f'Packet {loop_counter}; Sending {len(chunk)} bytes...')
        send_command(build_packet(command, [file_id] + chunk))
        loop_counter += 1
        time.sleep(1.2)

    print('Done!')

if __name__ == '__main__':

    # packet = [i for i in range(4)]
    # print(packet)
    # print(calc_checksum(packet))
    # print([int(x) for x in calc_checksum(packet)] + packet)
    # print(sum([int(x) for x in calc_checksum(packet)] + packet) & 0xFF)

    # print(load_auth_token('doorbell/auth_tokens.h'))

    # cmd_print_str('test string')

    cmd_play_audio(4)
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
