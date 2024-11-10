import asyncio
import aiohttp


async def main():
    async with aiohttp.ClientSession() as session:
        async with session.ws_connect("ws://localhost:5050/y/rare-drops/stream") as ws:
            async for msg in ws:
                if msg.type == aiohttp.WSMsgType.TEXT:
                    data = msg.json()
                    print(f"Received message: {data}")
                elif msg.type == aiohttp.WSMsgType.BINARY:
                    print(f"Received binary data: {msg.data}")
                elif msg.type == aiohttp.WSMsgType.CLOSE:
                    break
                elif msg.type == aiohttp.WSMsgType.ERROR:
                    break


if __name__ == "__main__":
    asyncio.run(main())
