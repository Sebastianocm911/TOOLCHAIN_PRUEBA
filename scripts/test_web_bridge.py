#!/usr/bin/env python3
"""Pruebas locales del extractor HTTPS/HTML y del encuadre TC/1."""
from ai_bridge import Decoder, PageText, frame, validate_https_url

def rejected(url):
    try:
        validate_https_url(url)
    except ValueError:
        return True
    return False

def main():
    assert rejected("http://example.com")
    assert rejected("https://127.0.0.1/")
    assert rejected("https://localhost/")
    parser = PageText()
    parser.feed("<html><head><title>Demo segura</title><script>alert(1)</script></head>"
                "<body><h1>Hola</h1><p>Texto &amp; prueba</p><style>.x{}</style></body></html>")
    title, body = parser.result()
    assert title == "Demo segura"
    assert "Hola" in body and "Texto & prueba" in body
    assert "alert" not in body and ".x" not in body
    packet = frame(6, 17, "https://example.com")
    decoded = Decoder().feed(packet)
    assert decoded == [(6, 17, b"https://example.com")]
    print("WEB BRIDGE: HTTPS policy, HTML extraction and TC/1 framing OK")

if __name__ == "__main__":
    main()
