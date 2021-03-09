import pyaudio
import socket
import wave

UDP_IP = "0.0.0.0"
UDP_PORT = 3005
WAVE_OUTPUT_FILENAME = "output.wav"
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

# p = pyaudio.PyAudio()

# stream = p.open(format=32, channels=1, rate=11111, input=True)

frames = []

try:
    while True:
        data, addr = sock.recvfrom(1024) # buffer de 1024 bytes
        print(addr)
        print(len(data))
        for i in range(0,len(data)):
            print(data[i])
            frames.append(data[i])
        
        # stream.write(data)
        
except KeyboardInterrupt:  
    print("Cerrando...")
    wf = wave.open(WAVE_OUTPUT_FILENAME, 'wb')
    wf.setnchannels(1)
    wf.setsampwidth(1)
    wf.setframerate(44100)
    wf.writeframes(b''.join(frames))
    wf.close()
    # stream.stop_stream()
    # stream.close()
    # p.terminate()
