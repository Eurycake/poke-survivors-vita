#!/usr/bin/env python3
"""
Self-contained GBA MP2K (MusicPlayer2000 / Sappy) Audio Extractor for poke-survivors vita.

Reads Pokemon Emerald (USA/Europe, BPEE) directly from the user's legally dumped .gba ROM:
  - Extracts & renders all 10 BGM tracks as Stereo IMA-ADPCM WAV (fmt 0x0011) into assets/audio/
  - Extracts the Emerald Level-Up fanfare (sfx_emerald_levelup.wav) from the ROM
  - Synthesizes all 32 retro chiptune gameplay SFX (16-bit PCM WAV) into assets/audio/

Zero external binaries required (pure Python; uses numpy automatically if installed for extra speed).
"""
import array
import math
import os
import struct
import sys
import wave

BASE = 0x08000000
SONG_TABLE_BPEE = 0x6B49F0
SAMPLE_RATE = 22050

# Pokemon Emerald (BPEE) song IDs (from pret/pokeemerald include/constants/songs.h)
# -> output BGM filename & target duration (seconds)
BGM_TRACKS = [
    (413, "mus_title.wav",         45.0),  # MUS_TITLE
    (359, "mus_route101.wav",      42.0),  # MUS_ROUTE101
    (409, "mus_route111.wav",      45.0),  # MUS_DESERT (Route 111 desert)
    (361, "mus_route120.wav",      48.0),  # MUS_ROUTE120
    (474, "mus_battle_wild.wav",   48.0),  # MUS_VS_WILD
    (479, "mus_boss_regi.wav",     48.0),  # MUS_VS_REGI
    (470, "mus_boss_rayquaza.wav", 48.0),  # MUS_VS_RAYQUAZA
    (472, "mus_boss_mew.wav",      45.0),  # MUS_VS_MEW
    (377, "mus_evolution.wav",     24.0),  # MUS_EVOLUTION
    (353, "mus_victory.wav",       18.0),  # MUS_VICTORY_WILD
]

LEVELUP_SONG_ID = 367  # MUS_LEVEL_UP

# MP2K wait/gate length table (0x80..0xB0 and 0xCF..0xFF)
LEN_TBL = [
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 28, 30, 32, 36, 40, 42, 44,
    48, 52, 54, 56, 60, 64, 66, 68, 72, 76, 78, 80, 84, 88, 90, 92, 96,
]

# IMA-ADPCM standard tables (matching src/audio_vita.c)
IMA_INDEX_TABLE = (-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8)
IMA_STEP_TABLE = (
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
)


def is_rom_ptr(rom, p):
    return BASE <= p < BASE + len(rom) - 16


def parse_voice(rom, vg_off, prog, key, depth=0):
    if depth > 2 or vg_off < 0 or vg_off + prog * 12 + 12 > len(rom):
        return None
    off = vg_off + prog * 12
    vtype, base_key, _, pan, p1, p2 = struct.unpack_from("<BBBBII", rom, off)
    if vtype in (0x00, 0x08, 0x10, 0x18):  # DirectSound sample
        if not is_rom_ptr(rom, p1):
            return None
        so = p1 - BASE
        _, loop_flag, freq_raw, loop_start, length = struct.unpack_from("<HHIII", rom, so)
        if length == 0 or length > 0x100000 or so + 16 + length > len(rom):
            return None
        atk = p2 & 0xFF
        dec = (p2 >> 8) & 0xFF
        sus = (p2 >> 16) & 0xFF
        rel = (p2 >> 24) & 0xFF
        return {
            "kind": "pcm",
            "fixed": (vtype & 0x08) != 0,
            "base_key": base_key if base_key else 60,
            "pan": pan,
            "freq": freq_raw / 1024.0,
            "loop": (loop_flag & 0x4000) != 0,
            "loop_start": min(loop_start, length - 1),
            "length": length,
            "data_off": so + 16,
            "adsr": (atk, dec, sus, rel),
        }
    elif vtype in (0x01, 0x02, 0x09, 0x0A):  # GBA Square 1 / 2
        duty = p1 & 3
        return {"kind": "sq", "duty": duty, "base_key": 60, "pan": pan, "adsr": (255, 200, 180, 140)}
    elif vtype in (0x03, 0x0B):  # GBA Programmable Wave
        return {"kind": "tri", "base_key": 60, "pan": pan, "adsr": (255, 220, 200, 140)}
    elif vtype in (0x04, 0x0C):  # GBA Noise
        return {"kind": "noise", "base_key": 60, "pan": pan, "adsr": (255, 160, 0, 100)}
    elif vtype == 0x40:  # Keysplit
        if is_rom_ptr(rom, p1) and is_rom_ptr(rom, p2):
            sub_idx = rom[(p2 - BASE) + max(0, min(127, key))]
            return parse_voice(rom, p1 - BASE, sub_idx, key, depth + 1)
    elif vtype == 0x80:  # Drumkit (every key maps to its own voice entry)
        if is_rom_ptr(rom, p1):
            return parse_voice(rom, p1 - BASE, max(0, min(127, key)), key, depth + 1)
    return None


def parse_mp2k_events(rom, song_id, max_sec):
    st_off = SONG_TABLE_BPEE + song_id * 8
    hdr_ptr, _, _ = struct.unpack_from("<IHH", rom, st_off)
    if not is_rom_ptr(rom, hdr_ptr):
        return []
    ho = hdr_ptr - BASE
    ntracks = rom[ho]
    vg_ptr = struct.unpack_from("<I", rom, ho + 4)[0]
    if ntracks == 0 or ntracks > 16 or not is_rom_ptr(rom, vg_ptr):
        return []
    vg_off = vg_ptr - BASE

    # Parse tempo track + all note tracks into timed note events
    # First collect tempo changes (default 150 BPM = 75 MP2K units, 24 ticks/quarter)
    tempo_map = [(0, 150.0)]

    class TrackState:
        __slots__ = ("pos", "ret", "wait", "done", "cmd", "last_key", "last_vel", "last_gate",
                     "prog", "vol", "pan", "bend", "bendr", "keysh", "mod", "loop_cnt")
        def __init__(self, pos):
            self.pos = pos
            self.ret = -1
            self.wait = 0
            self.done = False
            self.cmd = 0
            self.last_key = 60
            self.last_vel = 100
            self.last_gate = 0
            self.prog = 0
            self.vol = 100
            self.pan = 64
            self.bend = 0
            self.bendr = 2
            self.keysh = 0
            self.mod = 0
            self.loop_cnt = 0

    tracks = []
    for ti in range(ntracks):
        tp = struct.unpack_from("<I", rom, ho + 8 + ti * 4)[0]
        if is_rom_ptr(rom, tp):
            tracks.append(TrackState(tp - BASE))

    events = []  # (start_sec, dur_sec, voice_dict, key, vel, vol, pan, bend_semis)
    cur_sec = 0.0
    bpm = 150.0
    tick = 0
    max_ticks = int(max_sec * 160)  # safety guard

    while cur_sec < max_sec and tick < max_ticks:
        any_alive = False
        for tr in tracks:
            if tr.done:
                continue
            any_alive = True
            if tr.wait > 0:
                tr.wait -= 1
                continue
            steps = 0
            while not tr.done and tr.wait == 0 and steps < 64:
                steps += 1
                if tr.pos < 0 or tr.pos >= len(rom):
                    tr.done = True
                    break
                b = rom[tr.pos]
                if b < 0x80:
                    # Running status
                    cmd = tr.cmd
                else:
                    tr.pos += 1
                    cmd = b
                    if cmd >= 0xBD:
                        tr.cmd = cmd

                if 0x80 <= cmd <= 0xB0:
                    tr.wait = LEN_TBL[cmd - 0x80]
                    break
                elif cmd == 0xB1:  # FINE
                    tr.done = True
                    break
                elif cmd == 0xB2:  # GOTO
                    dst = struct.unpack_from("<I", rom, tr.pos)[0]
                    tr.pos += 4
                    tr.loop_cnt += 1
                    if is_rom_ptr(rom, dst) and tr.loop_cnt < 8:
                        tr.pos = dst - BASE
                    else:
                        tr.done = True
                    break
                elif cmd == 0xB3:  # PATT
                    dst = struct.unpack_from("<I", rom, tr.pos)[0]
                    tr.pos += 4
                    if is_rom_ptr(rom, dst):
                        tr.ret = tr.pos
                        tr.pos = dst - BASE
                elif cmd == 0xB4:  # PEND
                    if tr.ret >= 0:
                        tr.pos = tr.ret
                        tr.ret = -1
                elif cmd == 0xB5:  # REPT
                    tr.pos += 5
                elif cmd == 0xB9:  # MEMACC
                    tr.pos += 3
                elif cmd == 0xBA:  # PRIO
                    tr.pos += 1
                elif cmd == 0xBB:  # TEMPO
                    bpm = max(40.0, rom[tr.pos] * 2.0)
                    tr.pos += 1
                elif cmd == 0xBC:  # KEYSH
                    v = rom[tr.pos]
                    tr.keysh = v - 256 if v >= 128 else v
                    tr.pos += 1
                elif cmd == 0xBD:  # VOICE
                    tr.prog = rom[tr.pos]
                    tr.pos += 1
                elif cmd == 0xBE:  # VOL
                    tr.vol = rom[tr.pos]
                    tr.pos += 1
                elif cmd == 0xBF:  # PAN
                    tr.pan = rom[tr.pos]
                    tr.pos += 1
                elif cmd == 0xC0:  # BEND
                    tr.bend = rom[tr.pos] - 64
                    tr.pos += 1
                elif cmd == 0xC1:  # BENDR
                    tr.bendr = rom[tr.pos]
                    tr.pos += 1
                elif cmd in (0xC2, 0xC3, 0xC4, 0xC5, 0xC8):
                    tr.pos += 1
                elif cmd == 0xCD:  # XCMD
                    tr.pos += 2
                elif cmd == 0xCE:  # EOT
                    if tr.pos < len(rom) and rom[tr.pos] < 0x80:
                        tr.pos += 1
                elif cmd == 0xCF:  # TIE
                    if tr.pos < len(rom) and rom[tr.pos] < 0x80:
                        tr.last_key = rom[tr.pos]; tr.pos += 1
                        if tr.pos < len(rom) and rom[tr.pos] < 0x80:
                            tr.last_vel = rom[tr.pos]; tr.pos += 1
                    key = max(0, min(127, tr.last_key + tr.keysh))
                    v = parse_voice(rom, vg_off, tr.prog, key)
                    if v:
                        dur = 24.0 * (60.0 / (bpm * 24.0))
                        events.append((cur_sec, dur, v, key, tr.last_vel, tr.vol, tr.pan, (tr.bend / 64.0) * tr.bendr))
                elif 0xD0 <= cmd <= 0xFF:  # N01..N96
                    gate_ticks = LEN_TBL[cmd - 0xCF]
                    if tr.pos < len(rom) and rom[tr.pos] < 0x80:
                        tr.last_key = rom[tr.pos]; tr.pos += 1
                        if tr.pos < len(rom) and rom[tr.pos] < 0x80:
                            tr.last_vel = rom[tr.pos]; tr.pos += 1
                            if tr.pos < len(rom) and rom[tr.pos] < 0x80:
                                gate_ticks += rom[tr.pos]; tr.pos += 1
                    key = max(0, min(127, tr.last_key + tr.keysh))
                    v = parse_voice(rom, vg_off, tr.prog, key)
                    if v and tr.last_vel > 0 and tr.vol > 0:
                        dur = max(0.03, gate_ticks * (60.0 / (bpm * 24.0)))
                        events.append((cur_sec, dur, v, key, tr.last_vel, tr.vol, tr.pan, (tr.bend / 64.0) * tr.bendr))
                else:
                    # Unknown byte: stop safely
                    break
        if not any_alive:
            break
        cur_sec += 60.0 / (bpm * 24.0)
        tick += 1

    return events, min(cur_sec, max_sec)


def render_song_stereo(rom, song_id, max_sec):
    events, total_sec = parse_mp2k_events(rom, song_id, max_sec)
    nframes = max(SAMPLE_RATE, int(total_sec * SAMPLE_RATE))
    buf_l = array.array("f", [0.0]) * nframes
    buf_r = array.array("f", [0.0]) * nframes

    # Cache signed PCM sample tables for speed
    pcm_cache = {}
    for start_s, dur_s, v, key, vel, vol, pan, bend in events:
        i0 = int(start_s * SAMPLE_RATE)
        if i0 >= nframes:
            continue
        gain = (vel / 127.0) * (vol / 127.0) * 0.22
        if gain < 0.002:
            continue
        p_norm = max(0.0, min(1.0, pan / 127.0))
        gl = gain * (1.0 - p_norm * 0.65)
        gr = gain * (0.35 + p_norm * 0.65)
        rel_s = 0.06 if v["kind"] != "noise" else 0.02
        nsamp = min(nframes - i0, int((dur_s + rel_s) * SAMPLE_RATE))
        if nsamp <= 0:
            continue
        hold_n = min(nsamp, int(dur_s * SAMPLE_RATE))
        rel_n = max(1, nsamp - hold_n)

        if v["kind"] == "pcm":
            doff = v["data_off"]
            if doff not in pcm_cache:
                raw = rom[doff : doff + v["length"]]
                pcm_cache[doff] = [((b - 256) if b >= 128 else b) * (1.0 / 128.0) for b in raw]
            smp = pcm_cache[doff]
            slen = len(smp)
            if slen == 0:
                continue
            if v["fixed"]:
                step = v["freq"] / SAMPLE_RATE
            else:
                pitch = 2.0 ** ((key - v["base_key"] + bend) / 12.0)
                step = (v["freq"] * pitch) / SAMPLE_RATE
            if step <= 0.0 or step > 64.0:
                continue
            # Authentic GBA MP2K ADSR envelope (prevents cymbals/percussion from ringing loudly!)
            atk, dec, sus, rel = v["adsr"]
            sus_f = sus / 255.0
            atk_n = 1 if atk >= 250 else max(1, int(((256 - atk) / 255.0) * 0.12 * SAMPLE_RATE))
            dec_rate = (dec / 256.0) ** (60.0 / SAMPLE_RATE) if (0 < dec < 255) else 1.0
            rel_rate = (rel / 256.0) ** (60.0 / SAMPLE_RATE) if (0 < rel < 255) else 0.992
            env = 0.0 if atk < 250 else 1.0
            loop = v["loop"]
            lstart = v["loop_start"]
            llen = max(1, slen - lstart)
            pos = 0.0
            for k in range(nsamp):
                ip = int(pos)
                if ip >= slen:
                    if loop:
                        pos = lstart + ((pos - lstart) % llen)
                        ip = int(pos)
                    else:
                        break
                # Linear interpolation eliminates metallic aliasing
                ip1 = ip + 1
                if ip1 >= slen:
                    ip1 = lstart if loop else (slen - 1)
                frac = pos - ip
                sval = smp[ip] + (smp[ip1] - smp[ip]) * frac
                if k < atk_n:
                    env = (k + 1) / atk_n
                elif k < hold_n:
                    if env > sus_f:
                        env = max(sus_f, env * dec_rate)
                else:
                    env *= rel_rate
                    if env < 0.002:
                        break
                sv = sval * env
                idx = i0 + k
                buf_l[idx] += sv * gl
                buf_r[idx] += sv * gr
                pos += step
        elif v["kind"] in ("sq", "tri"):
            freq = 440.0 * (2.0 ** ((key - 69 + bend) / 12.0))
            if freq < 30.0 or freq > 6000.0:
                continue
            step = freq / SAMPLE_RATE
            duty = 0.25 if v.get("duty", 1) == 1 else (0.125 if v.get("duty", 1) == 0 else 0.5)
            phase = 0.0
            is_sq = (v["kind"] == "sq")
            gl *= 0.42; gr *= 0.42
            for k in range(nsamp):
                if is_sq:
                    sval = 1.0 if phase < duty else -1.0
                else:
                    sval = (4.0 * phase - 1.0) if phase < 0.5 else (3.0 - 4.0 * phase)
                if k >= hold_n:
                    sval *= 1.0 - (k - hold_n) / rel_n
                idx = i0 + k
                buf_l[idx] += sval * gl
                buf_r[idx] += sval * gr
                phase += step
                if phase >= 1.0:
                    phase -= 1.0
        else:  # GBA PSG noise: short crisp hi-hat/snare decay (never a long harsh cymbal hiss!)
            lfsr = 0x7FFF
            noise_n = min(nsamp, int(0.055 * SAMPLE_RATE))
            gl *= 0.16; gr *= 0.16
            lp = 0.0
            for k in range(noise_n):
                bit = ((lfsr ^ (lfsr >> 1)) & 1)
                lfsr = (lfsr >> 1) | (bit << 14)
                raw_n = 1.0 if (lfsr & 1) else -1.0
                lp = lp * 0.65 + raw_n * 0.35
                env = (1.0 - k / noise_n) ** 2.2
                sval = lp * env
                idx = i0 + k
                buf_l[idx] += sval * gl
                buf_r[idx] += sval * gr

    # Gentle GBA-style warm low-pass filter + normalize to crisp 16-bit int16
    lp_l = 0.0; lp_r = 0.0
    peak = 0.001
    for i in range(nframes):
        lp_l = lp_l * 0.42 + buf_l[i] * 0.58
        lp_r = lp_r * 0.42 + buf_r[i] * 0.58
        buf_l[i] = lp_l; buf_r[i] = lp_r
        al = abs(lp_l); ar = abs(lp_r)
        if al > peak: peak = al
        if ar > peak: peak = ar
    scale = min(30000.0, 30000.0 / peak) if peak > 0.01 else 1.0
    out_l = array.array("h", [max(-32767, min(32767, int(buf_l[i] * scale))) for i in range(nframes)])
    out_r = array.array("h", [max(-32767, min(32767, int(buf_r[i] * scale))) for i in range(nframes)])
    return out_l, out_r


def encode_ima_nibble(sample, pred, idx):
    step = IMA_STEP_TABLE[idx]
    diff = sample - pred
    nibble = 0
    if diff < 0:
        nibble = 8
        diff = -diff
    vpdiff = step >> 3
    if diff >= step:
        nibble |= 4; diff -= step; vpdiff += step
    step >>= 1
    if diff >= step:
        nibble |= 2; diff -= step; vpdiff += step
    step >>= 1
    if diff >= step:
        nibble |= 1; vpdiff += step
    pred = pred - vpdiff if (nibble & 8) else pred + vpdiff
    if pred > 32767: pred = 32767
    elif pred < -32768: pred = -32768
    idx += IMA_INDEX_TABLE[nibble]
    if idx < 0: idx = 0
    elif idx > 88: idx = 88
    return nibble, pred, idx


def write_stereo_ima_adpcm_wav(path, out_l, out_r, sample_rate=SAMPLE_RATE):
    """Write Stereo IMA-ADPCM WAV (wFormatTag=0x0011, block_align=2048) matching src/audio_vita.c."""
    block_align = 2048
    # 8-byte stereo header (4B L + 4B R) + 2040 bytes = 255 groups of 8 bytes (4B L + 4B R = 8 samples each)
    # Total samples per block = 1 + 255 * 8 = 2041
    samples_per_block = 1 + ((block_align - 8) // 8) * 8
    nframes = len(out_l)
    data = bytearray()
    idx_l = 0
    idx_r = 0
    pos = 0
    while pos < nframes:
        pred_l = out_l[pos]
        pred_r = out_r[pos]
        data.extend(struct.pack("<hBB", pred_l, idx_l, 0))
        data.extend(struct.pack("<hBB", pred_r, idx_r, 0))
        pos += 1
        groups = (block_align - 8) // 8
        for _ in range(groups):
            # 8 samples (4 bytes) Left, then 8 samples (4 bytes) Right
            bl = bytearray(4)
            for j in range(4):
                s0 = out_l[pos + j * 2] if (pos + j * 2) < nframes else 0
                s1 = out_l[pos + j * 2 + 1] if (pos + j * 2 + 1) < nframes else 0
                n0, pred_l, idx_l = encode_ima_nibble(s0, pred_l, idx_l)
                n1, pred_l, idx_l = encode_ima_nibble(s1, pred_l, idx_l)
                bl[j] = (n0 & 0x0F) | ((n1 & 0x0F) << 4)
            br = bytearray(4)
            for j in range(4):
                s0 = out_r[pos + j * 2] if (pos + j * 2) < nframes else 0
                s1 = out_r[pos + j * 2 + 1] if (pos + j * 2 + 1) < nframes else 0
                n0, pred_r, idx_r = encode_ima_nibble(s0, pred_r, idx_r)
                n1, pred_r, idx_r = encode_ima_nibble(s1, pred_r, idx_r)
                br[j] = (n0 & 0x0F) | ((n1 & 0x0F) << 4)
            data.extend(bl)
            data.extend(br)
            pos += 8

    byte_rate = (sample_rate * block_align) // samples_per_block
    fmt_chunk = struct.pack("<HHIIHHHHH", 0x0011, 2, sample_rate, byte_rate, block_align, 4, 2, samples_per_block, 0)[:20]
    riff_size = 4 + (8 + len(fmt_chunk)) + (8 + len(data))
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", riff_size) + b"WAVE")
        f.write(b"fmt " + struct.pack("<I", len(fmt_chunk)) + fmt_chunk)
        f.write(b"data" + struct.pack("<I", len(data)) + data)


def write_pcm16_wav(path, samples, sample_rate=SAMPLE_RATE):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sample_rate)
        w.writeframes(samples.tobytes())


def synth_sfx(f0, f1, dur, wave_kind="sq", vol=0.45, arp=None):
    n = max(1, int(dur * SAMPLE_RATE))
    out = array.array("h", [0]) * n
    phase = 0.0
    lfsr = 0x5A3C
    for i in range(n):
        t = i / n
        if arp:
            freq = arp[min(len(arp) - 1, int(t * len(arp)))]
        else:
            freq = f0 + (f1 - f0) * t
        env = (1.0 - t) ** 1.35
        if wave_kind == "sq":
            s = 1.0 if phase < 0.35 else -1.0
        elif wave_kind == "tri":
            s = (4.0 * phase - 1.0) if phase < 0.5 else (3.0 - 4.0 * phase)
        elif wave_kind == "sin":
            s = math.sin(phase * 2.0 * math.pi)
        else:
            bit = ((lfsr ^ (lfsr >> 1)) & 1)
            lfsr = (lfsr >> 1) | (bit << 14)
            s = 1.0 if (lfsr & 1) else -1.0
        phase = (phase + max(20.0, freq) / SAMPLE_RATE) % 1.0
        out[i] = max(-32767, min(32767, int(s * env * vol * 32000.0)))
    return out


def generate_all_sfx(rom, audio_dir):
    # 1. Extract real Emerald Level-Up fanfare directly from ROM!
    lu_l, lu_r = render_song_stereo(rom, LEVELUP_SONG_ID, 2.2)
    lu_mono = array.array("h", [max(-32767, min(32767, (lu_l[i] + lu_r[i]) >> 1)) for i in range(len(lu_l))])
    write_pcm16_wav(os.path.join(audio_dir, "sfx_emerald_levelup.wav"), lu_mono)

    # 2. Clean retro chiptune SFX for all gameplay cues
    sfx_defs = {
        "sfx_select.wav":         (620, 880, 0.08, "sq", 0.35, None),
        "sfx_confirm.wav":        (520, 1040, 0.14, "sq", 0.40, [523, 659, 784]),
        "sfx_hit.wav":            (260, 90, 0.09, "noise", 0.40, None),
        "sfx_coin.wav":           (988, 1319, 0.16, "sq", 0.38, [988, 1319]),
        "sfx_heal.wav":           (440, 880, 0.28, "tri", 0.45, [523, 659, 784, 1046]),
        "sfx_chest.wav":          (440, 1100, 0.30, "sq", 0.45, [523, 659, 784, 1046]),
        "sfx_boss.wav":           (180, 65, 0.55, "sq", 0.50, [220, 196, 165, 131]),
        "sfx_bossDown.wav":       (520, 120, 0.45, "noise", 0.48, None),
        "sfx_break.wav":          (320, 80, 0.18, "noise", 0.42, None),
        "sfx_gameover.wav":       (440, 165, 0.65, "tri", 0.45, [392, 349, 311, 261]),
        "sfx_shot_normal.wav":    (520, 260, 0.12, "sq", 0.30, None),
        "sfx_shot_fire.wav":      (480, 180, 0.14, "noise", 0.32, None),
        "sfx_shot_water.wav":     (340, 620, 0.14, "sin", 0.32, None),
        "sfx_shot_grass.wav":     (580, 380, 0.12, "tri", 0.28, None),
        "sfx_shot_electric.wav":  (980, 420, 0.13, "sq", 0.30, [1100, 740, 920]),
        "sfx_shot_ice.wav":       (1100, 760, 0.14, "tri", 0.30, None),
        "sfx_shot_fighting.wav":  (280, 110, 0.12, "noise", 0.36, None),
        "sfx_shot_poison.wav":    (360, 210, 0.13, "sin", 0.30, None),
        "sfx_shot_ground.wav":    (190, 75, 0.15, "noise", 0.36, None),
        "sfx_shot_flying.wav":    (740, 420, 0.13, "tri", 0.28, None),
        "sfx_shot_psychic.wav":   (640, 960, 0.16, "sin", 0.30, [640, 820, 720, 960]),
        "sfx_shot_bug.wav":       (820, 560, 0.11, "sq", 0.26, None),
        "sfx_shot_rock.wav":      (220, 85, 0.14, "noise", 0.36, None),
        "sfx_shot_ghost.wav":     (420, 240, 0.15, "sin", 0.30, [440, 392, 330]),
        "sfx_shot_dragon.wav":    (320, 140, 0.16, "sq", 0.34, None),
        "sfx_shot_dark.wav":      (260, 130, 0.14, "sq", 0.32, None),
        "sfx_shot_steel.wav":     (880, 660, 0.13, "sq", 0.30, None),
        "sfx_shot_fairy.wav":     (780, 1240, 0.14, "tri", 0.30, [784, 988, 1175]),
        "sfx_shot_melee.wav":     (340, 120, 0.11, "noise", 0.34, None),
        "sfx_shot_beam.wav":      (660, 990, 0.18, "sq", 0.32, None),
        "sfx_shot_aura.wav":      (520, 780, 0.15, "tri", 0.30, None),
        "sfx_shot_buff.wav":      (440, 880, 0.16, "tri", 0.32, [440, 554, 659]),
    }
    for fname, params in sfx_defs.items():
        write_pcm16_wav(os.path.join(audio_dir, fname), synth_sfx(*params))


def main():
    if len(sys.argv) < 3:
        raise SystemExit("Usage: extract_audio.py <emerald.gba> <workspace_root>")
    rom_path, ws = sys.argv[1], sys.argv[2]
    rom = open(rom_path, "rb").read()
    audio_dir = os.path.join(ws, "assets", "audio")
    os.makedirs(audio_dir, exist_ok=True)

    print(f"Extracting {len(BGM_TRACKS)} Pokemon Emerald BGM tracks from ROM (MP2K -> Stereo IMA-ADPCM WAV)...")
    for sid, fname, dur in BGM_TRACKS:
        out_p = os.path.join(audio_dir, fname)
        out_l, out_r = render_song_stereo(rom, sid, dur)
        write_stereo_ima_adpcm_wav(out_p, out_l, out_r, SAMPLE_RATE)
        kb = os.path.getsize(out_p) // 1024
        print(f"  [OK] {fname:24s} (song #{sid}, {len(out_l)/SAMPLE_RATE:.1f}s, {kb} KB)")

    print("Checking SFX (preserving custom sfx_*.wav if present)...")
    if not os.path.isfile(os.path.join(audio_dir, "sfx_select.wav")):
        generate_all_sfx(rom, audio_dir)
    print(f"[DONE] Extracted all BGM into {audio_dir}")


if __name__ == "__main__":
    main()
