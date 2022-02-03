from socket import *
from typing import *
import fcntl
import os


def create_client(serverIp: str, serverPort: int) -> Optional[socket]:
    """
    Creates a socket and attempts to connect it to port <serverPort>
    on the server with IP <serverIP>. If successful, return the socket
    otherwise return None.
    """
    clientSocket = socket(AF_INET, SOCK_STREAM)
    clientSocket.settimeout(5)

    try:
        clientSocket.connect((serverIp, serverPort))
    except Exception:
        print("Error creating the socket.\n")
        return None

    return clientSocket


if __name__ == "__main__":
    serverIp = '127.0.0.1'
    serverPort = 8098
