#BME 354 Pacemaker Added Functionality Bluetooth Terminal Code
#Authors: Kabir Gupta
#Last edited: April 21, 2026

import asyncio
from bleak import BleakClient
import csv
from datetime import datetime

#Address of the device
ADDRESS = "F5B376F0-2DD0-1D14-0E79-CEBD0EE44D53"

#Address of the HM-10 bluetooth module
CHAR_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb"

#Instantiate a buffer to assemble full lines
buffer = ""

#CSV logging
log_file = open("pacemaker_data.csv", "w", newline="")
writer = csv.writer(log_file)
writer.writerow(["timestamp", "time_ms", "HR", "Vs", "pacing"])

def handle_data(sender, data):
    global buffer
    try:
        #Decode incoming BLE data
        chunk = data.decode("utf-8")
        buffer += chunk

        #Process full lines only
        while "\n" in buffer:
            line, buffer = buffer.split("\n", 1)
            line = line.strip()
            print("Line:", line)

            #Trim headers
            if (
                "DATA" in line
                or "time" in line
                or "Vs" in line
                or len(line) == 0
            ):
                continue

            parts = line.split(',')
            if len(parts) == 4:
                try:
                    time_ms = float(parts[0])
                    hr = float(parts[1])
                    vs = float(parts[2])
                    pacing = int(parts[3])

                    #Output the lines of data cleanly
                    print(f"Parsed → Time: {time_ms} ms | HR: {hr} bpm | Vs: {vs} V | Pace: {pacing}")

                    #Save to CSV
                    now = datetime.now().strftime("%H:%M:%S")
                    writer.writerow([now, time_ms, hr, vs, pacing])
                    log_file.flush()

                except:
                    print("Skipping malformed line:", line)

    except Exception as e:
        print("Error:", e)


async def user_input_loop(client):
    while True:
        #Command line that will not block the received data
        cmd = await asyncio.to_thread(input, "Enter command: ")

        if cmd:
            try:
                await client.write_gatt_char(CHAR_UUID, (cmd + "\n").encode())
                print(f"Sent: {cmd}")
            except Exception as e:
                print("Send error:", e)


async def main():
    async with BleakClient(ADDRESS) as client:
        print("Connected to HM-10")
        #Print that the laptop is looking for data after connection started
        await client.start_notify(CHAR_UUID, handle_data)
        print("Listening for data...")
        await user_input_loop(client)

if __name__ == "__main__":
    asyncio.run(main())
