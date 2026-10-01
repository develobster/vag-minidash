"""Host Web API: consumes readings and saves host preferences, never initiates ECU traffic."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import threading
import time
import mimetypes
from pathlib import Path
from urllib.parse import urlsplit
from .configurator import Configurator, defaults, validate_config




class State:
    def __init__(self):
        self.lock = threading.Lock()
        self.data = {'status':'starting','signals':{},'signals_by_did':{}}

    def update(self, event):
        event = {"received_at": time.monotonic(), **event}
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

    def update_runtime(self, snapshot, *, ecus=(), can_state="unknown", vehicle_state="unknown"):
        """Publish only arbitrated values; never choose a source in the Web layer."""
        now = time.monotonic()
        rows = {}
        for name, decision in snapshot['decisions'].items():
            selected = decision.get('selected')
            rows[name] = ({**selected, 'received_at':now, 'initial_age':selected.get('age',0)}
                          if selected else dict(signal=name,value=None,quality='unavailable',source='unavailable'))
        with self.lock:
            self.data = dict(status='active', signals=rows, signals_by_did={},
                             metrics=snapshot['metrics'], ecus=list(ecus), can_state=can_state,
                             vehicle_state=vehicle_state)


def serve(state, port, *, host="127.0.0.1", data_dir=".minidash"):
    app = Configurator(state, data_dir)
    stop = threading.Event()
    assets = Path(__file__).with_name("web")
    class Handler(BaseHTTPRequestHandler):
        def reply(self, body, content_type="application/json; charset=utf-8", status=200):
            if not isinstance(body, bytes): body = json.dumps(body, ensure_ascii=False, allow_nan=False).encode()
            self.send_response(status)
            self.send_header('Content-Type',content_type)
            self.send_header('Cache-Control','no-store' if self.path.startswith('/api/') else 'no-cache')
            self.send_header('X-Content-Type-Options','nosniff')
            self.send_header('Content-Length',str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            path = urlsplit(self.path).path
            try:
                if path == '/api/events':
                    self.send_response(200)
                    self.send_header('Content-Type','text/event-stream')
                    self.send_header('Cache-Control','no-store')
                    self.send_header('X-Accel-Buffering','no')
                    self.end_headers()
                    while not stop.is_set():
                        body = json.dumps(app.telemetry(), ensure_ascii=False, allow_nan=False)
                        self.wfile.write(('data: '+body+'\n\n').encode()); self.wfile.flush()
                        stop.wait(.5)
                    return
                if path == '/api/signals': return self.reply(state.snapshot())
                if path == '/api/telemetry': return self.reply(app.telemetry())
                if path == '/api/configuration': return self.reply(app.configuration())
                if path == '/api/defaults': return self.reply(defaults())
                if path == '/api/logs': return self.reply(app.recordings())
                if path.startswith('/api/logs/'):
                    return self.reply(app.log_path(path.rsplit('/',1)[-1]).read_bytes(), 'application/x-ndjson')
                if path == '/api/ota': return self.reply(dict(available=False, reason="ESP32 Wi-Fi/OTA backend is not implemented"))
                name = 'index.html' if path == '/' else path.lstrip('/')
                if name not in {'index.html','app.js','model.js','style.css','sw.js','manifest.webmanifest','icon.svg','icon-192.png','icon-512.png'}:
                    return self.reply(dict(error='Not found'), status=404)
                file = assets / name
                return self.reply(file.read_bytes(), mimetypes.guess_type(name)[0] or 'application/octet-stream')
            except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError): pass
            except (ValueError, FileNotFoundError) as exc: self.reply(dict(error=str(exc)), status=404)

        def mutate(self):
            # Same-origin browser writes only; reject cross-site forms and fetches.
            origin = self.headers.get('Origin')
            if origin and origin != 'http://'+self.headers.get('Host',''):
                return self.reply(dict(error='Cross-origin write rejected'), status=403)
            if self.headers.get('Content-Type','').split(';')[0] != 'application/json':
                return self.reply(dict(error='JSON required'), status=415)
            try:
                size = int(self.headers.get('Content-Length','0'))
                if not 0 < size <= 262144: raise ValueError('Request must be 1–262144 bytes')
                body = json.loads(self.rfile.read(size))
                if not isinstance(body, dict): raise ValueError('JSON object required')
                path = urlsplit(self.path).path
                if path == '/api/configuration' and self.command == 'PUT': return self.reply(app.save(body))
                if path == '/api/configuration/validate' and self.command == 'POST':
                    validate_config(body['configuration']); return self.reply(dict(valid=True))
                if path == '/api/logs/start' and self.command == 'POST': return self.reply(app.start_recording())
                if path == '/api/logs/stop' and self.command == 'POST': return self.reply(app.stop_recording())
                if path == '/api/simulation' and self.command == 'POST' and getattr(state,'mode',None) == 'simulation':
                    if type(body.get('paused')) is not bool: raise ValueError('paused must be boolean')
                    state.paused = body['paused']; return self.reply(dict(paused=state.paused))
                if path.startswith('/api/logs/') and self.command == 'DELETE':
                    with app.lock:
                        file = app.log_path(path.rsplit('/',1)[-1]); file.unlink(); file.with_suffix('.json').unlink()
                    return self.reply(dict(deleted=True))
                return self.reply(dict(error='Capability unavailable'), status=404)
            except RuntimeError as exc: self.reply(dict(error=str(exc)), status=409)
            except (ValueError, KeyError, TypeError, FileNotFoundError) as exc: self.reply(dict(error=str(exc)), status=400)
            except OSError: self.reply(dict(error='Storage unavailable'), status=503)
        do_POST = mutate
        do_PUT = mutate
        do_DELETE = mutate
        def log_message(self,*args): pass
    class Server(ThreadingHTTPServer):
        daemon_threads = True
        def shutdown(self):
            stop.set()
            worker.join(2)
            if app.active: app.stop_recording()
            super().shutdown()
    server = Server((host,port),Handler)
    server.configurator = app
    def sample():
        while not stop.wait(.5):
            try: app.sample()
            except OSError:
                with app.lock:
                    app.recording_error = 'Recording stopped: storage unavailable'
                    if app.active:
                        try: app.active['file'].close()
                        except OSError: pass
                        app.active = None
    worker = threading.Thread(target=sample, daemon=True); worker.start()
    thread = threading.Thread(target=server.serve_forever,daemon=True)
    thread.start()
    return server,thread
