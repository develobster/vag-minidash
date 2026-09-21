"""Loopback read-only API/UI: consumes readings, never initiates diagnostic traffic."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import threading

PAGE = b'''<!doctype html><meta charset="utf-8"><title>MiniDash live research</title>
<h1>MiniDash live research</h1><p>Latest run state and raw readings; no vehicle controls.</p>
<pre id="data">Waiting...</pre><script>
async function refresh(){try{const r=await fetch('/api/signals');document.getElementById('data').textContent=JSON.stringify(await r.json(),null,2)}catch(e){document.getElementById('data').textContent='Disconnected'}}
refresh();setInterval(refresh,1000);</script>'''


class State:
    def __init__(self):
        self.lock = threading.Lock()
        self.data = {'status':'starting','signals':{},'signals_by_did':{}}

    def update(self, event):
        with self.lock:
            if event['kind'] == 'signal':
                name = event.get('canonical_name') or event.get('signal') or f"unknown.did.{event['did']}"
                self.data['signals'][name] = event
                self.data['signals_by_did'][event['did']] = name
            elif event['kind'] == 'state':
                self.data['status'] = event['status']
            elif event['kind'] == 'error':
                self.data['last_error'] = event
                if event.get('did'):
                    name = self.data['signals_by_did'].get(event['did'], f"unknown.did.{event['did']}")
                    self.data['signals'][name] = {**event,'canonical_name':name,'value':None,'quality':'error'}
                    self.data['signals_by_did'][event['did']] = name

    def snapshot(self):
        with self.lock:
            return json.dumps(self.data,ensure_ascii=False).encode()


def serve(state, port):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path not in ('/','/api/signals'):
                self.send_error(404)
                return
            body = PAGE if self.path == '/' else state.snapshot()
            self.send_response(200)
            self.send_header('Content-Type','text/html; charset=utf-8' if self.path == '/' else 'application/json; charset=utf-8')
            self.send_header('Cache-Control','no-store')
            self.send_header('Content-Length',str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        def log_message(self,*args): pass
    server = ThreadingHTTPServer(('127.0.0.1',port),Handler)
    thread = threading.Thread(target=server.serve_forever,daemon=True)
    thread.start()
    return server,thread
