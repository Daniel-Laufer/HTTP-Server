from typing import List, Tuple, Dict
from client import create_client
import os
import time
import socket
import os.path
import time


SERVER_IP = '127.0.0.1'
SERVER_PORT = 8098
HOST = f"{SERVER_IP}:{SERVER_PORT}"


def test_get_html() -> None:
    request = f"GET / HTTP/1.1\r\nHost: {HOST}\r\nConnection: keep-alive\r\n\r\n"
    client = create_client(SERVER_IP, SERVER_PORT)
    client.send(request.encode())
    res_headers, body = _parse_response(_read_response(client))

    # get the HTTP/1.0 <status code> <message> header
    res_http_response_code_header = res_headers[0]

    assert body != ''

    assert res_http_response_code_header == "HTTP/1.0 200 Success"

    # ensuring that we are recieving the entire exact index.html in the response.
    with open(f'{os.getcwd()}/website/index.html', 'r') as file:
        file_contents = file.read()
        file_contents = file_contents.replace("\n", "\r\n")
        assert file_contents == body
    client.close()


def test_get_css() -> None:
    request = f"GET /assets/index.css HTTP/1.1\r\nHost: {HOST}\r\nConnection: keep-alive\r\n\r\n"
    client = create_client(SERVER_IP, SERVER_PORT)
    client.send(request.encode())
    res_headers, body = _parse_response(_read_response(client))

    # get the HTTP/1.0 <status code> <message> header
    res_http_response_code_header = res_headers[0]

    assert body != ''

    assert res_http_response_code_header == "HTTP/1.0 200 Success"

    # ensuring that we are recieving the entire exact index.html in the response.
    with open(f'{os.getcwd()}/website/assets/index.css', 'r') as file:
        file_contents = file.read()
        file_contents = file_contents.replace("\n", "\r\n")
        assert file_contents == body


def test_getting_304_response() -> None:
    date = "Wed, 19 Nov 2025 09:23:50"
    request = f"GET / HTTP/1.1\r\nHost: {HOST}\r\nIf-Modified-Since: {date}\r\nConnection: keep-alive\r\n\r\n"
    client = create_client(SERVER_IP, SERVER_PORT)
    client.send(request.encode())
    res_headers, body = _parse_response(_read_response(client))
    # get the HTTP/1.0 <status code> <message> header
    res_http_response_code_header = res_headers[0]
    assert res_http_response_code_header == "HTTP/1.0 304 Not Modified"
    assert body == ''

    client.close()


def test_not_getting_304_response() -> None:
    date = "Wed, 19 Nov 2021 09:23:50"
    request = f"GET / HTTP/1.1\r\nHost: {HOST}\r\nIf-Modified-Since: {date}\r\nConnection: keep-alive\r\n\r\n"
    client = create_client(SERVER_IP, SERVER_PORT)
    client.send(request.encode())
    res_headers, body = _parse_response(_read_response(client))
    # get the HTTP/1.0 <status code> <message> header
    res_http_response_code_header = res_headers[0]
    assert body != ''

    assert res_http_response_code_header == "HTTP/1.0 200 Success"

    # ensuring that we are recieving the entire exact index.html in the response.
    with open(f'{os.getcwd()}/website/index.html', 'r') as file:
        file_contents = file.read()
        file_contents = file_contents.replace("\n", "\r\n")
        assert file_contents == body
    client.close()


def _read_response(client: socket.socket) -> str:
    """Read the response from the server in 256 byte increments.
    Converts the response to a string and returns it.
    """
    res_chunks = []
    while True:
        try:
            chunk = client.recv(256)
        # catching timeout exception
        except Exception as e:
            break

        if not chunk:
            break
        res_chunks.append(chunk.decode())

    assert len(res_chunks) > 0

    return ''.join(res_chunks)


# ============== Helper functions ==============
def _parse_response(response: str) -> Tuple[List[str], str]:
    """Returns a two-element tuple. The first element being a list of all
    the HTTP headers, the second being the body of the response.
    """
    # split the response up into the headers and the body
    res_split = response.split("\r\n\r\n")

    res_headers = res_split[0]
    res_body = res_split[1]

    # seperate each header
    res_headers_split = res_headers.split("\r\n")

    return res_headers_split, res_body


if __name__ == "__main__":
    test_get_css()
