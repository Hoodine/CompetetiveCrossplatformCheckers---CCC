import socket
import json
import struct
import threading
from collections import deque


class NetworkClient:
    def __init__(self):
        self.sock = None
        self.connected = False
        self.message_queue = deque()
        self.lock = threading.Lock()
        self.receive_thread = None
        self.running = False

    def connect(self, ip, port):
        try:
            print(f"Connecting to {ip}:{port}...")
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(5)
            self.sock.connect((ip, port))
            self.sock.settimeout(None)
            self.connected = True
            self.running = True
            self.receive_thread = threading.Thread(target=self._receive_loop, daemon=True)
            self.receive_thread.start()
            print("Connected successfully!")
            return True
        except Exception as e:
            print(f"Connection error: {e}")
            self.connected = False
            return False

    def disconnect(self):
        print("Disconnecting...")
        self.running = False
        if self.sock:
            try:
                self.sock.close()
            except:
                pass
        self.sock = None
        self.connected = False

    def send_message(self, msg_dict):
        if not self.connected or not self.sock:
            print("Cannot send: not connected")
            return False
        try:
            data = json.dumps(msg_dict).encode('utf-8')
            length = struct.pack('!I', len(data))
            self.sock.sendall(length + data)
            print(f"Sent: {msg_dict.get('type', 'unknown')}")
            return True
        except Exception as e:
            print(f"Send error: {e}")
            self.connected = False
            return False

    def poll_message(self):
        with self.lock:
            if self.message_queue:
                return self.message_queue.popleft()
            return None

    def _receive_loop(self):
        print("Receive loop started")
        while self.running and self.connected:
            try:
                length_bytes = self._recv_exact(4)
                if not length_bytes:
                    print("Failed to receive length, breaking")
                    break
                length = struct.unpack('!I', length_bytes)[0]
                if length > 65536:
                    print(f"Message too long: {length}")
                    continue

                data = self._recv_exact(length)
                if not data:
                    print("Failed to receive data, breaking")
                    break

                msg = json.loads(data.decode('utf-8'))
                print(f"Received: {msg.get('type', 'unknown')}")
                with self.lock:
                    self.message_queue.append(msg)
            except Exception as e:
                print(f"Receive error: {e}")
                break

        print("Receive loop ended")
        self.connected = False
        self.running = False

    def _recv_exact(self, size):
        data = b''
        while len(data) < size:
            try:
                if not self.sock:
                    return None
                chunk = self.sock.recv(size - len(data))
                if not chunk:
                    return None
                data += chunk
            except socket.timeout:
                continue
            except Exception as e:
                print(f"recv error: {e}")
                return None
        return data