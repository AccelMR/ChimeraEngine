"""Writes the example's sounds as 16-bit mono WAV files next to this script.

Plain Python (no packages), like generate_images.py, so the example needs no sound files of
unknown license. The WAV files are not kept in the repository: the assets in /Game/Audio hold
them whole, and running this again makes them to import anew.
"""
import math
import os
import random
import struct
import wave

RATE = 44100
HERE = os.path.dirname(os.path.abspath(__file__))


def write(name, samples, rate=RATE):
    path = os.path.join(HERE, name)
    with wave.open(path, 'wb') as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(rate)
        out.writeframes(b''.join(
            struct.pack('<h', int(max(-1.0, min(1.0, s)) * 32767)) for s in samples))
    print('wrote', path, '%.2f s' % (len(samples) / rate))


def square(phase, duty=0.5):
    return 1.0 if (phase % 1.0) < duty else -1.0


def triangle(phase):
    p = phase % 1.0
    return 4.0 * p - 1.0 if p < 0.5 else 3.0 - 4.0 * p


def sweep(seconds, start_hz, end_hz, wave_fn, decay, volume):
    """A tone whose pitch slides from start_hz to end_hz and fades out."""
    count = int(seconds * RATE)
    phase = 0.0
    samples = []
    for i in range(count):
        t = i / count
        hz = start_hz * (end_hz / start_hz) ** t
        phase += hz / RATE
        samples.append(wave_fn(phase) * volume * math.exp(-decay * t) * min(1.0, i / 40))
    return samples


def noise_burst(seconds, cutoff_start, cutoff_end, decay, volume, rumble_hz):
    """Low passed noise with a low tone under it: an explosion."""
    random.seed(7)
    count = int(seconds * RATE)
    low = 0.0
    phase = 0.0
    samples = []
    for i in range(count):
        t = i / count
        cutoff = cutoff_start * (cutoff_end / cutoff_start) ** t
        alpha = 1.0 - math.exp(-2.0 * math.pi * cutoff / RATE)
        low += alpha * (random.uniform(-1.0, 1.0) - low)
        phase += rumble_hz * (1.0 - 0.6 * t) / RATE
        tone = math.sin(2.0 * math.pi * phase) * 0.5
        samples.append((low * 2.2 + tone) * volume * math.exp(-decay * t) * min(1.0, i / 30))
    return samples


def music(rate):
    """A loop of eight bars: bass, an arpeggio and a hat, ending where it starts. Half the
    rate of the effects is plenty for it and keeps the file small."""
    bpm = 112
    beat = 60.0 / bpm
    bars = 8
    count = int(bars * 4 * beat * rate)
    # A minor, F, C, G, twice.
    chords = [(57, 60, 64), (53, 57, 60), (48, 52, 55), (55, 59, 62)] * 2

    def hz(note):
        return 440.0 * 2.0 ** ((note - 69) / 12.0)

    random.seed(3)
    samples = [0.0] * count
    sixteenth = beat / 4
    for i in range(count):
        time = i / rate
        bar = int(time / (4 * beat)) % bars
        chord = chords[bar]
        step = int(time / sixteenth)
        in_step = (time - step * sixteenth) / sixteenth
        # Bass on the eighths, an octave below the root.
        bass_note = chord[0] - 12 if (step // 2) % 2 == 0 else chord[0] - 5
        bass = triangle(time * hz(bass_note)) * 0.32 * (1.0 - 0.5 * ((step % 2) * 0.5 + in_step * 0.5))
        # Arpeggio on the sixteenths.
        arp_note = chord[step % 3] + 12
        arp = square(time * hz(arp_note), 0.25) * 0.07 * math.exp(-4.0 * in_step)
        # Hat on the off eighths.
        hat = random.uniform(-1.0, 1.0) * 0.05 * math.exp(-30.0 * in_step) if step % 4 == 2 else 0.0
        samples[i] = bass + arp + hat
    # Short fades so the loop point never clicks.
    fade = int(0.005 * rate)
    for i in range(fade):
        samples[i] *= i / fade
        samples[-1 - i] *= i / fade
    return samples


if __name__ == '__main__':
    write('shot_player.wav', sweep(0.09, 1400.0, 500.0, lambda p: square(p, 0.3), 3.5, 0.35))
    write('shot_enemy.wav', sweep(0.18, 520.0, 260.0, triangle, 2.5, 0.5))
    write('explosion.wav', noise_burst(0.7, 2400.0, 180.0, 4.0, 0.55, 70.0))
    write('hurt.wav', sweep(0.35, 300.0, 90.0, lambda p: square(p, 0.5), 2.0, 0.4))
    write('music.wav', music(RATE // 2), RATE // 2)
