from pathlib import Path
import asyncio
import csv
import json
import math

from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError
from config import (
    BLUETOOTH_INTERFACE, SERVICE_UUID, MEASUREMENT_UUID, ACK_UUID,
    DEVICE_NAME_PREFIX, SCAN_TIMEOUT_S, MEASUREMENT_TIMEOUT_S
)

CSV_FILE = Path(__file__).resolve().parent / "gateway.csv"
CSV_FIELDS = ["node_id", "timestamp", "temperature", "ph", "turbidity"]


def ensure_csv():
    if not CSV_FILE.exists():
        with CSV_FILE.open("w", newline="", encoding="utf-8") as f:
            csv.DictWriter(f, fieldnames=CSV_FIELDS).writeheader()


def valid_number(x):
    return isinstance(x, (int, float)) and not isinstance(x, bool) and math.isfinite(x)


def validate(data):
    for field in CSV_FIELDS:
        if field not in data:
            return False
    if not isinstance(data["node_id"], str):
        return False
    if not isinstance(data["timestamp"], str):
        return False
    if not valid_number(data["temperature"]):
        return False
    for field in ("ph", "turbidity"):
        if data[field] is not None and not valid_number(data[field]):
            return False
    return True


def store(data):
    row = dict(data)
    if row["ph"] is None: row["ph"] = "NA"
    if row["turbidity"] is None: row["turbidity"] = "NA"
    with CSV_FILE.open("a", newline="", encoding="utf-8") as f:
        csv.DictWriter(f, fieldnames=CSV_FIELDS).writerow(row)


async def handle_device(device):
    print(f"\nConectando a {device.name} [{device.address}]...")
    
    try:
        async with BleakClient(
            device,
            bluez={"adapter": BLUETOOTH_INTERFACE},
            timeout=15.0
        ) as client:
            if not client.is_connected:
                print(f"Falha ao conectar com {device.name}.")
                return

            print(f"Conectado a {device.name}. Aguardando medição...")

            event = asyncio.Event()
            received = {}

            def notification_callback(_, data):
                try:
                    received["data"] = json.loads(bytes(data).decode("utf-8"))
                    event.set()
                except (UnicodeDecodeError, json.JSONDecodeError) as err:
                    print(f"Payload JSON corrompido recebido: {err}")

            await client.start_notify(MEASUREMENT_UUID, notification_callback)
            
            try:
                await asyncio.wait_for(event.wait(), timeout=MEASUREMENT_TIMEOUT_S)
            except asyncio.TimeoutError:
                print(f"Timeout aguardando medição de {device.name}.")
                return
            finally:
                # Garantir que a notificação seja parada se a conexão ainda estiver ativa
                try:
                    if client.is_connected:
                        await client.stop_notify(MEASUREMENT_UUID)
                except Exception:
                    pass

            data = received.get("data")

            if not data or not validate(data):
                print("Medição rejeitada (dados inválidos).")
                try:
                    await client.write_gatt_char(ACK_UUID, b"ERROR", response=True)
                except Exception as e:
                    print(f"Erro ao enviar ACK (ERROR): {e}")
                return

            # Grava os dados no arquivo CSV
            store(data)
            
            # Envia confirmação de sucesso para o nó
            try:
                await client.write_gatt_char(ACK_UUID, b"OK", response=True)
                print(f"Recebido, armazenado e ACK (OK) enviado: {data['node_id']} [{data['timestamp']}]")
            except Exception as e:
                print(f"Erro ao enviar ACK (OK): {e}")

    except BleakError as exc:
        print(f"Erro de BLE ao processar {device.name}: {exc}")
    except Exception as exc:
        print(f"Erro inesperado em {device.name}: {exc}")


async def main():
    ensure_csv()
    print(f"Iniciando Gateway BLE contínuo na interface [{BLUETOOTH_INTERFACE}]...")
    print("Pressione Ctrl+C para interromper.\n")

    while True:
        try:
            print("Procurando nós ICC316 via BLE...")
            devices = await BleakScanner.discover(
                timeout=SCAN_TIMEOUT_S,
                bluez={"adapter": BLUETOOTH_INTERFACE}
            )

            candidates = [
                d for d in devices
                if d.name and d.name.startswith(DEVICE_NAME_PREFIX)
            ]

            if not candidates:
                print("Nenhum nó ICC316 encontrado neste ciclo.")
            else:
                print(f"Encontrado(s) {len(candidates)} nó(s). Processando...")
                for device in candidates:
                    await handle_device(device)

        except BleakError as exc:
            print(f"Erro no adaptador Bluetooth durante a busca: {exc}")
            # Aguarda alguns segundos para o stack BlueZ se recuperar
            await asyncio.sleep(5)
        except Exception as exc:
            print(f"Erro inesperado no loop principal: {exc}")
            await asyncio.sleep(5)

        # Breve pausa entre os ciclos de varredura
        await asyncio.sleep(2)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nGateway finalizado pelo usuário.")
