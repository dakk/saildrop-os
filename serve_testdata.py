import argparse
import asyncio

class SentenceHandler:
    def __init__(self, sentences, interval, start_index):
        self.sentences = sentences
        self.interval = interval
        self.current_index = start_index

    def get_next_sentence(self):
        sentence = self.sentences[self.current_index % len(self.sentences)]
        self.current_index += 1
        return sentence

class TCPHandler(SentenceHandler):
    async def handle_client(self, reader, writer):
        client_id = id(writer)
        print(f"New TCP client connected: {client_id}")
        try:
            while True:
                sentence = self.get_next_sentence()
                writer.write(sentence.encode())
                await writer.drain()
                await asyncio.sleep(self.interval)
        except (ConnectionResetError, BrokenPipeError):
            print(f"TCP client {client_id} disconnected. Will resume from sentence {self.current_index} when reconnected.")
        finally:
            writer.close()
            await writer.wait_closed()

class UDPHandler(SentenceHandler):
    def __init__(self, sentences, interval, start_index, port):
        super().__init__(sentences, interval, start_index)
        self.port = port
        self.clients = set()

    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, data, addr):
        if addr not in self.clients:
            print(f"New UDP client connected: {addr}")
            self.clients.add(addr)
        # We don't process incoming data, just use it as a "keepalive"

    async def send_sentences(self):
        while True:
            sentence = self.get_next_sentence()
            for addr in list(self.clients):
                try:
                    self.transport.sendto(sentence.encode(), addr)
                except Exception as e:
                    print(f"Error sending to UDP client {addr}: {e}")
                    self.clients.remove(addr)
            await asyncio.sleep(self.interval)

async def run_tcp_server(handler, port):
    server = await asyncio.start_server(
        handler.handle_client,
        '0.0.0.0', port
    )
    addr = server.sockets[0].getsockname()
    print(f'TCP server running on {addr}')
    async with server:
        await server.serve_forever()

async def run_udp_server(handler, port):
    loop = asyncio.get_running_loop()
    transport, protocol = await loop.create_datagram_endpoint(
        lambda: handler,
        local_addr=('0.0.0.0', port)
    )
    print(f'UDP server running on 0.0.0.0:{port}')
    await handler.send_sentences()

async def main(file_path, tcp_port, udp_port, interval, start_index):
    with open(file_path, 'r') as file:
        sentences = file.readlines()

    tcp_handler = TCPHandler(sentences, interval, start_index)
    udp_handler = UDPHandler(sentences, interval, start_index, udp_port)

    await asyncio.gather(
        run_tcp_server(tcp_handler, tcp_port),
        run_udp_server(udp_handler, udp_port)
    )

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="NMEA Sentence Sender")
    parser.add_argument("file", help="Path to the NMEA sentence file")
    parser.add_argument("--tcp-port", type=int, default=2000, help="TCP port to listen on")
    parser.add_argument("--udp-port", type=int, default=2001, help="UDP port to listen on")
    parser.add_argument("--interval", type=float, default=1.0, help="Interval between sentences in seconds")
    parser.add_argument("--start", type=int, default=0, help="Starting sentence index")

    args = parser.parse_args()

    asyncio.run(main(args.file, args.tcp_port, args.udp_port, args.interval, args.start))