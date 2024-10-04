import argparse
import asyncio
import time

import argparse
import asyncio
import time

class ClientHandler:
    def __init__(self, sentences, interval, start_index):
        self.sentences = sentences
        self.interval = interval
        self.current_index = start_index
        self.clients = set()

    async def handle_client(self, reader, writer):
        client_id = id(writer)
        self.clients.add(client_id)
        print(f'Client {client_id} connected, starting from {self.current_index}')
        try:
            while True:
                sentence = self.sentences[self.current_index % len(self.sentences)]
                print (f'Sending sentence => {sentence.strip()}')
                writer.write(sentence.encode())
                await writer.drain()
                self.current_index += 1
                await asyncio.sleep(self.interval)
        except (ConnectionResetError, BrokenPipeError):
            print(f"Client {client_id} disconnected at sentence {self.current_index}")
        finally:
            self.clients.remove(client_id)
            writer.close()
            try:
                await writer.wait_closed()
            except BrokenPipeError:
                pass
                

async def main(file_path, port, interval, start_index):
    with open(file_path, 'r') as file:
        sentences = file.readlines()

    handler = ClientHandler(sentences, interval, start_index)

    server = await asyncio.start_server(
        handler.handle_client,
        '0.0.0.0', port
    )

    addr = server.sockets[0].getsockname()
    print(f'Serving on {addr}')

    async with server:
        await server.serve_forever()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="NMEA Sentence Sender")
    parser.add_argument("file", help="Path to the NMEA sentence file")
    parser.add_argument("--port", type=int, default=3000, help="Port to listen on")
    parser.add_argument("--interval", type=float, default=1.0, help="Interval between sentences in seconds")
    parser.add_argument("--start", type=int, default=0, help="Starting sentence index")

    args = parser.parse_args()

    asyncio.run(main(args.file, args.port, args.interval, args.start))