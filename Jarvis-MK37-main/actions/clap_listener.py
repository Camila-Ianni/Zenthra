"""
clap_listener.py — Detección de aplausos para activar JARVIS
=============================================================
Escucha dos aplausos consecutivos y activa el inicio de JARVIS.
Reproduce "Should I Stay or Should I Go" mientras inicia.
"""

import numpy as np
import sounddevice as sd
import threading
import time
from pathlib import Path

# ── Configuración ─────────────────────────────────────────────────────────────
SAMPLE_RATE = 44100
THRESHOLD = 0.75  # Umbral de volumen para detectar aplauso (0-1) - menos sensible
CLAP_WINDOW = 2.5  # Ventana de tiempo entre aplausos (segundos)
COOLDOWN = 5.0  # Tiempo de espera después de detectar (evita falsos positivos)


class ClapListener:
    """Escucha aplausos y activa JARVIS cuando detecta dos consecutivos."""

    def __init__(self, activation_callback):
        self.activation_callback = activation_callback
        self.last_clap_time = 0
        self.clap_count = 0
        self.running = False
        self.cooldown_end = 0
        self._stream = None
        self._base_volume = 0.1  # Volumen base de referencia

    def _detect_clap(self, audio_data):
        """Detecta si hay un aplauso en los datos de audio."""
        # Calcular el pico máximo y el volumen RMS
        max_amplitude = np.max(np.abs(audio_data))
        rms = np.sqrt(np.mean(audio_data**2))
        
        # Detectar si hay un pico significativo sobre el volumen base
        return max_amplitude > THRESHOLD or rms > 0.5

    def _audio_callback(self, indata, frames, time_info, status):
        """Callback del stream de audio."""
        if status:
            return  # Ignorar errores de stream

        current_time = time.time()

        # Verificar cooldown
        if current_time < self.cooldown_end:
            return

        if self._detect_clap(indata):
            time_since_last = current_time - self.last_clap_time

            if 0.2 < time_since_last < CLAP_WINDOW:
                # Segundo aplauso detectado!
                self.clap_count = 2
                self.last_clap_time = current_time
                self.cooldown_end = current_time + COOLDOWN

                print("👏👏 ¡Dos aplausos detectados! Activando JARVIS...")

                # Activar en un hilo separado para no bloquear el audio
                threading.Thread(
                    target=self.activation_callback,
                    daemon=True
                ).start()

            elif time_since_last >= CLAP_WINDOW:
                # Reiniciar conteo si pasó mucho tiempo
                self.clap_count = 1
                self.last_clap_time = current_time
                print("👏 Primer aplauso detectado...")

            else:
                # Primer aplauso
                self.clap_count = 1
                self.last_clap_time = current_time
                print("👏 Primer aplauso detectado...")

    def start(self):
        """Inicia la escucha de aplausos."""
        self.running = True

        def run_stream():
            try:
                self._stream = sd.InputStream(
                    samplerate=SAMPLE_RATE,
                    channels=1,
                    dtype='float32',
                    blocksize=1024,
                    callback=self._audio_callback
                )
                self._stream.start()
                print("🎤 Escuchando aplausos... (da dos aplausos para activar)")

                while self.running:
                    sd.sleep(100)

            except Exception as e:
                print(f"❌ Error en ClapListener: {e}")

        threading.Thread(target=run_stream, daemon=True).start()

    def stop(self):
        """Detiene la escucha de aplausos."""
        self.running = False
        if self._stream:
            self._stream.stop()
            self._stream.close()


def play_activation_song():
    """
    Reproduce 'Should I Stay or Should I Go' de The Clash.
    Intenta reproducción directa en Spotify y, si falla, usa búsqueda.
    """
    import subprocess
    import platform
    from urllib.parse import quote

    song_name = "Should I Stay or Should I Go"
    artist = "The Clash"
    search_query = quote(f"{artist} {song_name}")
    track_uri = "spotify:track:39shmbIHICJ2Wxnk1fPSdz"
    track_web_url = "https://open.spotify.com/track/39shmbIHICJ2Wxnk1fPSdz"

    system = platform.system()

    try:
        if system == "Darwin":  # macOS
            print(f"🎵 Reproduciendo en Spotify: {song_name} - {artist}")

            # Intentar reproducción directa por URI
            result = subprocess.run([
                "open", "-a", "Spotify",
                track_uri
            ], check=False, timeout=3, capture_output=True)

            if result.returncode != 0:
                # Fallback web por si no está registrada la URI
                subprocess.run([
                    "open", track_web_url
                ], check=False, timeout=3, capture_output=True)

            print(f"🎵 Reproduciendo: {song_name} - {artist}")

        elif system == "Windows":
            # Intentar reproducción directa por URI
            subprocess.run([
                "powershell", "-Command",
                f'Start-Process "{track_uri}"'
            ], check=False, timeout=3)
            print(f"🎵 Reproduciendo: {song_name} - {artist}")

        else:  # Linux
            # Intentar reproducción directa por URI
            subprocess.run([
                "xdg-open", track_uri
            ], check=False, timeout=3)
            print(f"🎵 Reproduciendo: {song_name} - {artist}")

    except Exception as e:
        print(f"⚠️ No se pudo abrir Spotify automáticamente: {e}")
        print(f"   🎶 Busca manualmente: https://open.spotify.com/search/{search_query}")


# ── Función principal de activación ───────────────────────────────────────────
def on_clap_detected(ui=None):
    """
    Callback cuando se detectan dos aplausos.
    Reproduce la canción e inicia JARVIS.
    """
    print("\n" + "="*50)
    print("🎸 ACTIVANDO JARVIS CON APLAUSOS 🎸")
    print("="*50)

    # Reproducir la canción
    play_activation_song()

    # Pequeña pausa dramática
    time.sleep(1)

    # El callback de activación se encargará de iniciar JARVIS
    if ui:
        ui.write_log("👏👏 Activado por aplausos")


# ── Test standalone ───────────────────────────────────────────────────────────
if __name__ == "__main__":
    print("🎤 Test de ClapListener")
    print("Da dos aplausos para probar la activación...\n")

    listener = ClapListener(lambda: on_clap_detected())
    listener.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n🔴 Deteniendo ClapListener...")
        listener.stop()
