import http.server
import socketserver
import argparse

PATH = "/ota.bin"
BINARY_FILE = "a.bin"

def get_file_data(path):   
    if path == PATH:
        try:
            with open(BINARY_FILE, "rb") as f:
                data = f.read()
            ctype = http.server.SimpleHTTPRequestHandler.guess_type(http.server.SimpleHTTPRequestHandler, BINARY_FILE)
            return data, ctype
        except Exception as ex:
            print("Error opening binary file:", ex)
            raise ex
    return None, None

class BasicHTTPHandler(http.server.BaseHTTPRequestHandler):
    

    def do_HEAD(self):
        try:
            _, ctype = get_file_data(self.path)
        except:
            self.send_response(500)
            self.send_header("Content-type", "text/html")
            self.end_headers()
            return
        
        if ctype:
            self.send_response(200)
            self.send_header("Content-type", ctype)
            self.end_headers()
        else:
            self.send_response(404)
            self.send_header("Content-type", "text/html")
            self.end_headers()
            
        
    def do_GET(self):
        try:
            data, ctype = get_file_data(self.path)
        except:
            self.send_response(500)
            self.send_header("Content-type", "text/html")
            self.end_headers()
            self.wfile.write(b"<!DOCTYPE html><html><head></head><body>")
            self.wfile.write(b"500: Internal Server Error")
            self.wfile.write(b"</body></html>")
            return
        
        if ctype:
            self.send_response(200)
            self.send_header("Content-type", ctype)
            self.end_headers()
            self.wfile.write(data)
        else:
            self.send_response(404)
            self.send_header("Content-type", "text/html")
            self.end_headers()
            self.wfile.write(b"<!DOCTYPE html><html><head></head><body>")
            self.wfile.write(b"404: File not found")
            self.wfile.write(b"</body></html>")

def run_http_server(binary_file, http_filename="ota.bin", port=8070):
    global PATH, BINARY_FILE
    PATH = "/" + http_filename
    BINARY_FILE = binary_file

    with socketserver.TCPServer(("", port), BasicHTTPHandler) as httpd:
        print(f"Serving {binary_file} as {PATH} at port {port}")
        httpd.serve_forever()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("binary_file")
    parser.add_argument("-p", "--port", type=int,
                        default=8070,
                        help="The port to run the http server on, default: 8070")
    parser.add_argument("-f", "--http-filename",
                        default="ota.bin",
                        help="The http path of the binary file, default: ota.bin")
    args = parser.parse_args()

    run_http_server(args.binary_file, args.http_filename, args.port)
