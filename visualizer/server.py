#!/usr/bin/env python3
import json
import os
import shlex
import signal
import subprocess
import sys
import threading
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PAGE = Path(__file__).resolve().parent / "index.html"
TEXT_PATH = "data/texto.txt"
HOST = "127.0.0.1"
PORT = int(os.environ.get("PORT", "8765"))
PROGRAMS = ("secuencial", "naive", "intercalado", "ciclico", "maestro_trabajador", "hibrido")
CHUNKED = ("ciclico", "maestro_trabajador")
ALLOWED_HOSTS = {f"127.0.0.1:{PORT}", f"localhost:{PORT}"}
ALLOWED_ORIGINS = {f"http://{host}" for host in ALLOWED_HOSTS}

run_lock = threading.Lock()
active = {"process": None}


class BadRequest(Exception):
    pass


def bounded_int(params, name, low, high, default=None):
    raw = params.get(name, default)
    if raw is None:
        raise BadRequest(f"Falta el parámetro «{name}».")
    text = str(raw).strip()
    if not text.isdigit():
        raise BadRequest(f"«{name}» debe ser un entero no negativo.")
    value = int(text)
    if value < low or value > high:
        raise BadRequest(f"«{name}» debe estar entre {low} y {high}.")
    return value


def build_command(params):
    program = params.get("program")
    if program not in PROGRAMS:
        raise BadRequest("Programa desconocido.")
    bits = bounded_int(params, "bits", 8, 56)
    key = bounded_int(params, "key", 0, (1 << bits) - 1)
    processes = bounded_int(params, "processes", 1, 8, 4)
    threads = bounded_int(params, "threads", 1, 4, 2)
    chunk = bounded_int(params, "chunk", 8, 30, 16)
    limit = bounded_int(params, "limit", 1, 3600, 120)
    if program == "maestro_trabajador" and processes < 2:
        raise BadRequest("El maestro-trabajador necesita al menos 2 procesos.")
    if program in CHUNKED and chunk >= bits:
        raise BadRequest("El chunk (-c) debe ser menor que los bits (-b).")

    binary = ROOT / "bin" / program
    if not binary.exists():
        raise BadRequest(f"No existe bin/{program}: corre «make» primero.")

    args = ["-f", TEXT_PATH, "-k", str(key), "-b", str(bits), "-t", str(limit), "-v"]
    if program in CHUNKED:
        args += ["-c", str(chunk)]
    env = os.environ.copy()
    prefix = ""
    if program == "secuencial":
        command = [f"./bin/{program}", *args]
    elif program == "hibrido":
        env["OMP_NUM_THREADS"] = str(threads)
        prefix = f"OMP_NUM_THREADS={threads} "
        command = ["mpirun", "--bind-to", "none", "-x", "OMP_NUM_THREADS",
                   "-np", str(processes), f"./bin/{program}", *args]
    else:
        command = ["mpirun", "-np", str(processes), f"./bin/{program}", *args]
    return command, env, prefix + shlex.join(command)


def parse_line(line):
    fields = line.split(",")
    try:
        if fields[0] == "PROGRESS" and len(fields) == 4:
            return {"t": "p", "w": int(fields[1]), "i": int(fields[2]), "s": float(fields[3])}
        if fields[0] == "CHUNK" and len(fields) == 4:
            return {"t": "c", "w": int(fields[1]), "k": fields[2], "s": float(fields[3])}
        if fields[0] == "CSV" and len(fields) == 11:
            return {
                "t": "r",
                "program": fields[1],
                "processes": int(fields[2]),
                "threads": int(fields[3]),
                "bits": int(fields[4]),
                "key": fields[5],
                "found": fields[6],
                "finder": int(fields[7]),
                "iterations": int(fields[8]),
                "total": int(fields[9]),
                "time": float(fields[10]),
                "csv": line,
            }
    except ValueError:
        pass
    return {"t": "l", "text": line}


def stop_process(process):
    if process is None or process.poll() is not None:
        return
    try:
        os.killpg(process.pid, signal.SIGTERM)
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass


def cpu_model():
    try:
        for line in Path("/proc/cpuinfo").read_text().splitlines():
            if line.startswith("model name"):
                return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return ""


class Handler(BaseHTTPRequestHandler):
    server_version = "BruteForceVisualizer/1.0"

    def log_message(self, format, *args):
        pass

    def trusted(self):
        if self.headers.get("Host") not in ALLOWED_HOSTS:
            return False
        origin = self.headers.get("Origin")
        return origin is None or origin in ALLOWED_ORIGINS

    def send_json(self, status, payload):
        body = json.dumps(payload).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if not self.trusted():
            self.send_error(HTTPStatus.FORBIDDEN)
            return
        path = self.path.split("?", 1)[0]
        if path in ("/", "/index.html"):
            body = PAGE.read_bytes()
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)
        elif path == "/api/info":
            self.send_json(HTTPStatus.OK, {
                "programs": [p for p in PROGRAMS if (ROOT / "bin" / p).exists()],
                "cpu": cpu_model(),
                "cores": os.cpu_count(),
                "text": TEXT_PATH,
            })
        else:
            self.send_error(HTTPStatus.NOT_FOUND)

    def do_POST(self):
        if not self.trusted() or self.headers.get("Content-Type", "").split(";")[0] != "application/json":
            self.send_error(HTTPStatus.FORBIDDEN)
            return
        path = self.path.split("?", 1)[0]
        if path == "/api/stop":
            stop_process(active["process"])
            self.send_json(HTTPStatus.OK, {"stopped": True})
        elif path == "/api/run":
            self.run_program()
        else:
            self.send_error(HTTPStatus.NOT_FOUND)

    def run_program(self):
        try:
            length = int(self.headers.get("Content-Length", "0"))
            params = json.loads(self.rfile.read(min(length, 4096)) or b"{}")
            if not isinstance(params, dict):
                raise BadRequest("Cuerpo inválido.")
            command, env, display = build_command(params)
        except (BadRequest, json.JSONDecodeError, ValueError) as error:
            self.send_json(HTTPStatus.BAD_REQUEST, {"error": str(error)})
            return

        if not run_lock.acquire(blocking=False):
            self.send_json(HTTPStatus.CONFLICT, {"error": "Ya hay un programa corriendo."})
            return
        process = None
        try:
            process = subprocess.Popen(
                command, cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                stdin=subprocess.DEVNULL, text=True, bufsize=1, start_new_session=True)
            active["process"] = process
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", "application/x-ndjson; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.emit({"t": "start", "command": display})
            for raw in process.stdout:
                line = raw.rstrip("\n")
                if line:
                    self.emit(parse_line(line))
            code = process.wait()
            self.emit({"t": "exit", "code": code})
        except (BrokenPipeError, ConnectionResetError):
            pass
        except OSError as error:
            try:
                self.emit({"t": "l", "text": f"Error al lanzar: {error}"})
                self.emit({"t": "exit", "code": -1})
            except OSError:
                pass
        finally:
            stop_process(process)
            active["process"] = None
            run_lock.release()

    def emit(self, payload):
        self.wfile.write((json.dumps(payload) + "\n").encode())
        self.wfile.flush()


def main():
    if not PAGE.exists():
        sys.exit(f"No se encontró {PAGE}")
    missing = [p for p in PROGRAMS if not (ROOT / "bin" / p).exists()]
    if missing:
        print(f"Aviso: faltan binarios ({', '.join(missing)}); corre «make» en {ROOT}.")
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    server.daemon_threads = True
    print(f"Visualizador listo en http://{HOST}:{PORT}  (Ctrl+C para salir)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        stop_process(active["process"])
        server.server_close()


if __name__ == "__main__":
    main()
