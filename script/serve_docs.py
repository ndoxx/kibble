#!/usr/bin/python3

import os, sys, argparse
import http.server
import socketserver

# Small HTTP server to make the docs accessible on the local network
# For proofreading purposes

def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--port', default=8042, help='Select server port (default to 8042)')
    args = parser.parse_args()
    PORT = int(args.port)

    # Change cwd to the doxygen folder
    selfdir = os.path.dirname(os.path.realpath(__file__))
    docpath = os.path.join(selfdir, "../doxygen/html")
    os.chdir(docpath)

    Handler = http.server.SimpleHTTPRequestHandler
    with socketserver.TCPServer(("", PORT), Handler) as httpd:
        print("serving at port", PORT)
        httpd.serve_forever()


if __name__ == '__main__':
    main(sys.argv[1:])

