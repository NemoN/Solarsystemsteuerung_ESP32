"""Startet einen Archivdienst auf mehreren ausdrücklich angegebenen Adressen."""
import os
import socket

import uvicorn


def bind_sockets(hosts, port):
    sockets = []
    try:
        for host in hosts:
            sockets.append(socket.create_server((host, port), backlog=2048))
        return sockets
    except BaseException:
        for listener in sockets:
            listener.close()
        raise


def main():
    hosts = [host.strip() for host in os.getenv("HISTORY_LISTEN_HOSTS", "127.0.0.1").split(",")]
    if any(not host for host in hosts) or len(hosts) != len(set(hosts)):
        raise ValueError("HISTORY_LISTEN_HOSTS muss unterschiedliche Adressen enthalten.")
    port = int(os.getenv("HISTORY_PORT", "8088"))
    sockets = bind_sockets(hosts, port)
    try:
        uvicorn.Server(uvicorn.Config("app.main:app", workers=1)).run(sockets=sockets)
    finally:
        for listener in sockets:
            listener.close()


if __name__ == "__main__":
    main()
